/**
 * @copyright 2017 otaUpdate.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @author Christopher Armenio
 */
#include "ota_httpClient.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/platform.h"
#include <mbedtls/debug.h>
#include "mbedtls/net.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include "ota_logging.h"
#include "ota_misc.h"


// ******** local macro definitions ********
#define TAG										"ota_httpClient"
#define FORMATTED_BUFFERLEN_BYTES				48
#define HTTP_RESPONSE_LINE_BUFFERLEN_BYTES		92


// ******** local type definitions ********


// ******** local function prototypes ********
static void closeConnection(void);
static bool writeString(const char *const stringIn);
static bool writeLine(const char *const lineIn);
static bool writeFormattedLine(const char* formatIn, ...);
static bool readResponseLine(uint8_t* lineBufferIn, const size_t lineBufferMaxSize_bytesIn, size_t *const lineLength_bytesOut);
static bool parseStatusCode(uint16_t *const statusOut);
static bool parseContentLength(size_t *const contentLength_bytesOut);
static bool forwardToStartOfBody(void);
static bool readBody(size_t contentLengthIn, char* bodyOut);


// ********  local variable declarations *********
static bool isInit = false;

static char* hostname;
static uint16_t portNum;

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static mbedtls_ssl_context ssl;
static mbedtls_x509_crt cacert;
static mbedtls_ssl_config conf;
static mbedtls_net_context server_fd;


// ******** global function implementations ********
bool ota_httpClient_init(const char *const hostnameIn, const uint16_t portNumIn, const unsigned char *rootCaCertIn, size_t rootCaCertSize_bytesIn)
{
	if( hostnameIn == NULL ) return false;

	// save our references / internal state
	isInit = false;
	hostname = (char*)hostnameIn;
	portNum = portNumIn;

	OTA_LOG_DEBUG(TAG, "beginning initialization");

	// initialize our basic ssl modules
	int tmpRet;
	mbedtls_ssl_init(&ssl);
	mbedtls_x509_crt_init(&cacert);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_ssl_config_init(&conf);

	OTA_LOG_DEBUG(TAG, "initializing TLS entropy");
	mbedtls_entropy_init(&entropy);
	if( (tmpRet = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0)) != 0 )
	{
		OTA_LOG_ERROR(TAG, "mbedtls_ctr_drbg_seed returned %d", tmpRet);
		return false;
	}

	OTA_LOG_DEBUG(TAG, "loading root CA certificate");
	tmpRet = mbedtls_x509_crt_parse(&cacert, rootCaCertIn, rootCaCertSize_bytesIn);
	if( tmpRet < 0 )
	{
		OTA_LOG_ERROR(TAG, "mbedtls_x509_crt_parse returned %s0x%x", tmpRet<0?"-":"", tmpRet<0?-(unsigned)tmpRet:tmpRet);
		return false;
	}

	OTA_LOG_DEBUG(TAG, "setting TLS hostname");
	if( (tmpRet = mbedtls_ssl_set_hostname(&ssl, hostname)) != 0 )
	{
		OTA_LOG_ERROR(TAG, "mbedtls_ssl_set_hostname returned %s0x%x", tmpRet<0?"-":"", tmpRet<0?-(unsigned)tmpRet:tmpRet);
		return false;
	}

	OTA_LOG_DEBUG(TAG, "configuring TLS defaults");
	if( (tmpRet = mbedtls_ssl_config_defaults(&conf,
	                                          MBEDTLS_SSL_IS_CLIENT,
	                                          MBEDTLS_SSL_TRANSPORT_STREAM,
	                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0 )
	{
		OTA_LOG_ERROR(TAG, "mbedtls_ssl_config_defaults returned %d", tmpRet);
		return false;
	}

	OTA_LOG_DEBUG(TAG, "configuring host verification");
	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);


	mbedtls_esp_enable_debug_log(&conf, 4);


	OTA_LOG_DEBUG(TAG, "configuring TLS");
	if( (tmpRet = mbedtls_ssl_setup(&ssl, &conf)) != 0 )
	{
		OTA_LOG_ERROR(TAG, "mbedtls_ssl_setup returned %s0x%x", tmpRet<0?"-":"", tmpRet<0?-(unsigned)tmpRet:tmpRet);
		return false;
	}

	// if we made it here, we were successful
	OTA_LOG_DEBUG(TAG, "initialization successful");
	isInit = true;
	return true;
}


bool ota_httpClient_postJson(const char *const urlIn, const char *const bodyIn,
							 uint16_t *const statusOut, char *const bodyOut, const size_t bodyOutMaxSize_bytesIn)
{
	if( !isInit )
	{
		OTA_LOG_ERROR(TAG, "not initialized");
		return false;
	}

	// setup our network connection
	mbedtls_net_init(&server_fd);

	// create our port number string
	char portNum_str[6];
	snprintf(portNum_str, sizeof(portNum_str), "%d", portNum);
	portNum_str[sizeof(portNum_str)-1] = 0;

	OTA_LOG_INFO(TAG, "connecting to %s:%s...", hostname, portNum_str);
	int tmpRet;
	if( (tmpRet = mbedtls_net_connect(&server_fd, hostname, portNum_str, MBEDTLS_NET_PROTO_TCP)) != 0 )
	{
		OTA_LOG_ERROR(TAG, "mbedtls_net_connect returned %s0x%x", tmpRet<0?"-":"", tmpRet<0?-(unsigned)tmpRet:tmpRet);
		mbedtls_ssl_session_reset(&ssl);
		mbedtls_net_free(&server_fd);
		return false;
	}


	OTA_LOG_DEBUG(TAG, "performing TLS handshake...");
	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
	while( (tmpRet = mbedtls_ssl_handshake(&ssl)) != 0 )
	{
		if( (tmpRet != MBEDTLS_ERR_SSL_WANT_READ) && (tmpRet != MBEDTLS_ERR_SSL_WANT_WRITE) )
		{
			OTA_LOG_ERROR(TAG, "mbedtls_ssl_handshake returned %s0x%x", tmpRet<0?"-":"", tmpRet<0?-(unsigned)tmpRet:tmpRet);
			closeConnection();
			return false;
		}
	}


	OTA_LOG_DEBUG(TAG, "verifying peer X.509 certificate...");
	int flags;
	if( (flags = mbedtls_ssl_get_verify_result(&ssl)) != 0 )
	{
		OTA_LOG_ERROR(TAG, "failed to verify peer certificate");
		closeConnection();
		return false;
	}


	OTA_LOG_DEBUG(TAG, "writing headers");
	if( !writeFormattedLine("POST %s HTTP/1.1", urlIn) ||
		!writeFormattedLine("Host: %s", hostname) ||
		!writeLine("Content-Type: application/json") ||
		!writeFormattedLine("Content-Length: %d", strlen(bodyIn)) ||
		!writeString("\r\n") )
	{
		OTA_LOG_ERROR(TAG, "failed to write headers");
		closeConnection();
		return false;
	}

	OTA_LOG_DEBUG(TAG, "writing body");
	if( !writeString(bodyIn) )
	{
		OTA_LOG_ERROR(TAG, "failed to write body");
		closeConnection();
		return false;
	}


	OTA_LOG_DEBUG(TAG, "reading response");

	uint16_t statusCode;
	if( !parseStatusCode(&statusCode) )
	{
		OTA_LOG_ERROR(TAG, "failed to read status code");
		closeConnection();
		return false;
	}
	OTA_LOG_DEBUG(TAG, "statusCode: %d", statusCode);

	size_t contentLength_bytes;
	if( !parseContentLength(&contentLength_bytes) )
	{
		OTA_LOG_ERROR(TAG, "failed to read content length");
		closeConnection();
		return false;
	}
	OTA_LOG_DEBUG(TAG, "contentLength: %d", contentLength_bytes);

	if( !forwardToStartOfBody() ||
		((contentLength_bytes+1) > bodyOutMaxSize_bytesIn) ||
		!readBody(contentLength_bytes, bodyOut) )
	{
		OTA_LOG_ERROR(TAG, "problem reading body");
		closeConnection();
		return false;
	}

	OTA_LOG_DEBUG(TAG, "done reading response");
	closeConnection();

	// if we made it here we were successful...save our status code and return
	if( statusOut != NULL ) *statusOut = statusCode;
	return true;
}


// ******** local function implementations ********
static void closeConnection(void)
{
	mbedtls_ssl_close_notify(&ssl);
	mbedtls_ssl_session_reset(&ssl);
	mbedtls_net_free(&server_fd);
}


static bool writeString(const char *const stringIn)
{
	size_t bufferSize_bytesIn = strlen(stringIn);

//	for(int i = 0; i < bufferSize_bytesIn; i++)
//	{
//		putchar(stringIn[i]);
//	}

	int tmpRet;
	unsigned char* buf = (unsigned char*)stringIn;
	do
	{
		tmpRet = mbedtls_ssl_write(&ssl, (const unsigned char*)buf, bufferSize_bytesIn);
		if( tmpRet > 0 )
		{
			buf += tmpRet;
			bufferSize_bytesIn -= tmpRet;
		}
		else if( (tmpRet != MBEDTLS_ERR_SSL_WANT_WRITE) && (tmpRet != MBEDTLS_ERR_SSL_WANT_READ) )
		{
			OTA_LOG_ERROR(TAG, "error during write: %d", tmpRet);
			return false;
		}
	} while( bufferSize_bytesIn > 0 );

	return true;
}


static bool writeLine(const char *const lineIn)
{
	return writeString(lineIn) && writeString("\r\n");
}


static bool writeFormattedLine(const char* formatIn, ...)
{
	// our buffer for this go-round
	char buff[FORMATTED_BUFFERLEN_BYTES];

	va_list varArgs;
	va_start(varArgs, formatIn);

	// now do our VARARGS
	size_t expectedNumBytesWritten = vsnprintf(buff, sizeof(buff), formatIn, varArgs);
	if( sizeof(buff) >= expectedNumBytesWritten )
	{
		// our buffer fits the string...write it
		if( !writeString(buff) ) return false;
	}

	va_end(varArgs);

	return writeString("\r\n");
}


static bool readResponseLine(uint8_t* lineBufferIn, const size_t lineBufferMaxSize_bytesIn, size_t *const lineLength_bytesOut)
{
	if( lineBufferIn == NULL ) return false;

	memset(lineBufferIn, 0, lineBufferMaxSize_bytesIn);
	size_t currBufferLen_bytes = 0;

	int tmpRet;
	while( currBufferLen_bytes < lineBufferMaxSize_bytesIn )
	{
		tmpRet = mbedtls_ssl_read(&ssl, &lineBufferIn[currBufferLen_bytes++], 1);
		if( (tmpRet == MBEDTLS_ERR_SSL_WANT_READ) || (tmpRet == MBEDTLS_ERR_SSL_WANT_WRITE) ) continue;

		// check for errors
		if( tmpRet < 0 )
		{
			OTA_LOG_ERROR(TAG, "mbedtls_ssl_read returned -0x%x", -tmpRet);
			return false;
		}

		// check for socket closing
		if( (tmpRet == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) ||
			(tmpRet == 0) )
		{
			OTA_LOG_DEBUG(TAG, "connection closed");
			return false;
		}

		// if we made it here, we successfully stored a piece of data...check for CRLF
		if( (currBufferLen_bytes >= 2) && (lineBufferIn[currBufferLen_bytes-2] == '\r') && (lineBufferIn[currBufferLen_bytes-1] == '\n') )
		{
			lineBufferIn[currBufferLen_bytes-2] = 0;
			lineBufferIn[currBufferLen_bytes-1] = 0;
			break;
		}
	}

	// if we made it here, we were successful
	if( lineLength_bytesOut != NULL ) *lineLength_bytesOut = currBufferLen_bytes - 2;
	return true;
}


static bool parseStatusCode(uint16_t *const statusOut)
{
	uint8_t lineBuffer[HTTP_RESPONSE_LINE_BUFFERLEN_BYTES];
	size_t lineLength_bytes;

	// should always be the _first_ line
	memset(lineBuffer, 0, sizeof(lineBuffer));
	lineLength_bytes = 0;

	if( !readResponseLine(lineBuffer, sizeof(lineBuffer), &lineLength_bytes) ) return false;

	// if we made it here, we got a line...parse it
	char* save_ptr;
	char* currToken = strtok_r((char*)lineBuffer, " ", &save_ptr);
	if( currToken == NULL ) return false;

	// first token should start with 'HTTP'
	const char httpStr[] = "HTTP";
	if( strncmp(currToken, httpStr, strlen(httpStr)) != 0 ) return false;

	// next token should be status code
	currToken = strtok_r(NULL, " ", &save_ptr);
	if( currToken == NULL ) return false;

	char* endPtr;
	long statusCode = strtol(currToken, &endPtr, 10);
	if( *endPtr != '\0' ) return false;

	if( statusOut != NULL ) *statusOut = (uint16_t)statusCode;

	return true;
}


static bool parseContentLength(size_t *const contentLength_bytesOut)
{
	uint8_t lineBuffer[HTTP_RESPONSE_LINE_BUFFERLEN_BYTES];
	size_t lineLength_bytes;

	const char contentLenStr[] = "Content-Length:";
	while(1)
	{
		memset(lineBuffer, 0, sizeof(lineBuffer));
		lineLength_bytes = 0;

		if( !readResponseLine(lineBuffer, sizeof(lineBuffer), &lineLength_bytes) ) return false;

		// always look for end of headers
		if( strlen((char*)lineBuffer) == 0 ) return false;

		// if we made it here we got a line...see if it starts with the right string
		if( strncmp((char*)lineBuffer, contentLenStr, strlen(contentLenStr)) == 0 ) break;
	}

	// if we made it here, we got the right line...parse it
	char* save_ptr;
	char* currToken = strtok_r((char*)lineBuffer, " ", &save_ptr);
	if( currToken == NULL ) return false;

	// one more tokenization should give us the value
	currToken = strtok_r(NULL, " ", &save_ptr);
	if( currToken == NULL ) return false;

	long contentLength = strtol(currToken, NULL, 10);


	if( contentLength_bytesOut != NULL ) *contentLength_bytesOut = (size_t)contentLength;

	return true;
}


static bool forwardToStartOfBody(void)
{
	uint8_t lineBuffer[HTTP_RESPONSE_LINE_BUFFERLEN_BYTES];
	size_t lineLength_bytes;

	while(1)
	{
		memset(lineBuffer, 0, sizeof(lineBuffer));
		lineLength_bytes = 0;

		if( !readResponseLine(lineBuffer, sizeof(lineBuffer), &lineLength_bytes) ) return false;

		// look for end of headers
		if( strlen((char*)lineBuffer) == 0 ) break;
	}

	return true;
}


static bool readBody(size_t contentLengthIn, char* bodyOut)
{
	if( bodyOut == NULL ) return false;

	memset(bodyOut, 0, contentLengthIn+1);		// +1 for NULL-term

	int tmpRet;
	for( size_t i = 0; i < contentLengthIn; i++ )
	{
		tmpRet = mbedtls_ssl_read(&ssl, (uint8_t*)&bodyOut[i++], 1);
		if( (tmpRet == MBEDTLS_ERR_SSL_WANT_READ) || (tmpRet == MBEDTLS_ERR_SSL_WANT_WRITE) ) continue;

		// check for errors
		if( tmpRet < 0 )
		{
			OTA_LOG_ERROR(TAG, "mbedtls_ssl_read returned -0x%x", -tmpRet);
			return false;
		}

		// check for socket closing
		if( (tmpRet == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) ||
			(tmpRet == 0) )
		{
			OTA_LOG_DEBUG(TAG, "connection closed");
			return false;
		}
	}

	return true;
}
