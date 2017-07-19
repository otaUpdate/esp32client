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
#include "ota_updateClient.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ota_cert_ca.h"
#include "ota_flash.h"
#include "ota_httpClient.h"
#include "ota_logging.h"
#include "ota_misc.h"
#include "ota_thread.h"


// ******** local macro definitions ********
#define OTA_HOSTNAME						"devs.otaupdate.net"
#define OTA_PORTNUM							443

#define OTA_POLL_STARTUP_DELAY_MS			5 * 1000
#define OTA_POLL_PERIOD_MS					10 * 1000
#define INTERBLOCK_DELAY_MS					0

#define UUID_LEN_BYTES						36
#define SERIALNUM_MAXLEN_BYTES				UUID_LEN_BYTES+1
#define FWSIZESTR_MAXLEN_BYTES				9

#define FW_DL_MAX_FAILED_TRIES				8

#define TAG									"ota_updateClient"

#define DIV_ROUND_UP(num, denom)			(((num) + ((denom) - 1)) / (denom))


// ******** local type definitions ********
typedef struct
{
	ota_updateClient_updateAvailableCb_t cb_updateAvailable;
	ota_updateClient_willUpdateCb_t cb_willUpdate;
	void* userVar;
}listenerEntry_t;


// ******** local function prototypes ********
static void mainThread(void);
static bool isUpdateAvailable(char* updateUuidOut, size_t *const fwSize_bytesOut);
static void downloadUpdateWithUuid(char *targetUuidIn, size_t fwSize_bytesIn);


// ********  local variable declarations *********
static char fwUuid[UUID_LEN_BYTES+1];
static char hwUuid[UUID_LEN_BYTES+1];
static char devSerialNum[SERIALNUM_MAXLEN_BYTES];

static listenerEntry_t listeners[OTA_UPDATECLIENT_MAXNUM_CBS];
static size_t numListeners = 0;

static ota_updateClient_loggingFunction_t logFunction = NULL;
static void* logFunction_userVar = NULL;


// ******** global function implementations ********
bool ota_updateClient_init(const char *const fwUuidIn, const char *const hwUuidIn, const char *const devSerialNumIn)
{
	// ensure we have our needed parameters
	if( fwUuidIn == NULL ) return false;
	if( hwUuidIn == NULL ) return false;
	if( devSerialNumIn == NULL ) return false;

	// store them locally
	if( !ota_copyStringToBufferSafely(fwUuidIn, fwUuid, sizeof(fwUuid)) ) return false;
	if( !ota_copyStringToBufferSafely(hwUuidIn, hwUuid, sizeof(hwUuid)) ) return false;
	if( !ota_copyStringToBufferSafely(devSerialNumIn, devSerialNum, sizeof(devSerialNum)) ) return false;

	// schedule our thread
	ota_thread_startThreadWithCallback(mainThread);

	// if we made it here, we were successful
	return true;
}


void ota_updateClient_setLogFunction(ota_updateClient_loggingFunction_t loggingFunctionIn, void *userVarIn)
{
	logFunction = loggingFunctionIn;
	logFunction_userVar = userVarIn;
}


bool ota_updateClient_addListener(ota_updateClient_updateAvailableCb_t cb_updateAvailableIn,
								  ota_updateClient_willUpdateCb_t cb_willUpdateIn,
								  void* userVarIn)
{
	if( numListeners >= (sizeof(listeners)/sizeof(*listeners)) ) return false;

	listeners[numListeners].cb_updateAvailable = cb_updateAvailableIn;
	listeners[numListeners].cb_willUpdate = cb_willUpdateIn;
	listeners[numListeners].userVar = userVarIn;
	numListeners++;

	return true;
}


void ota_updateClient_log(int otaLogLevelIn, const char *const tagIn, const char *const fmtIn, ...)
{
	if( logFunction == NULL ) return;

	// now do our VARARGS
	va_list varArgs;
	va_start(varArgs, fmtIn);
	logFunction(logFunction_userVar, otaLogLevelIn, tagIn, fmtIn, varArgs);
	va_end(varArgs);
}


// ******** local function implementations ********
static void mainThread(void)
{
	ota_thread_delay_ms(OTA_POLL_STARTUP_DELAY_MS);
	OTA_LOG_INFO(TAG, "current FW UUID: %s", fwUuid);
	ota_flash_printPartitionInfo();

	// initialize our http client
	if( !ota_httpClient_init(OTA_HOSTNAME, OTA_PORTNUM, (const unsigned char*)OTA_CA_CERT, sizeof(OTA_CA_CERT)) ) return;

	while(1)
	{
		// wait for the next polling period
		OTA_LOG_DEBUG(TAG, "next update check in %d seconds", OTA_POLL_PERIOD_MS/1000);
		ota_thread_delay_ms(OTA_POLL_PERIOD_MS);
		OTA_LOG_INFO(TAG, "checking for updates now");

		char targetUuid[UUID_LEN_BYTES+1];
		size_t firmwareSize_bytes;
		if( isUpdateAvailable(targetUuid, &firmwareSize_bytes) )
		{
			OTA_LOG_INFO(TAG, "new FW available: '%s'", targetUuid);
			downloadUpdateWithUuid(targetUuid, firmwareSize_bytes);
		}
		else
		{
			OTA_LOG_INFO(TAG, "FW up-to-date");
		}
	}
}


static bool isUpdateAvailable(char* updateUuidOut, size_t *const fwSize_bytesOut)
{
	if( (updateUuidOut == NULL) || (fwSize_bytesOut == NULL) ) return false;

	// create our check-in request body...
	char body[57];
	snprintf(body, sizeof(body), "{\"currFwUuid\":\"%s\"}", fwUuid);
	body[sizeof(body)-1] = 0;

	// send our check request
	uint16_t httpStatus;
	size_t responseLen_bytes;
	char responseBody[UUID_LEN_BYTES+1+FWSIZESTR_MAXLEN_BYTES+1];		// +1 for separating space, +1 for null-term
	if( !ota_httpClient_postJson("/v1/checkforupdate", body, &httpStatus, responseBody, sizeof(responseBody), &responseLen_bytes, false) )
	{
		OTA_LOG_WARN(TAG, "update check failed, will retry");
		return false;
	}

	// if we made it here, we got a valid HTTP response...verify it
	if( httpStatus != 200 )
	{
		OTA_LOG_WARN(TAG, "HTTP request did not return 200(OK), will retry");
		return false;
	}

	// if we made it here, update check was successful, see if we have an update
	if( responseLen_bytes >= (UUID_LEN_BYTES+1+1) )			// +1 for separating space, +1 for min firmware size
	{
		// copy out our UUID
		strncpy(updateUuidOut, responseBody, UUID_LEN_BYTES);
		updateUuidOut[UUID_LEN_BYTES] = 0;

		// copy out our fw size
		char* endPtr;
		*fwSize_bytesOut = strtol(&responseBody[UUID_LEN_BYTES], &endPtr, 10);
		if( *endPtr != '\0' )
		{
			OTA_LOG_WARN(TAG, "error parsing firmware length");
			return false;
		}

		// notify our listeners
		for( size_t i = 0; i < numListeners; i++ )
		{
			if( listeners[i].cb_updateAvailable != NULL ) listeners[i].cb_updateAvailable(responseBody, listeners[i].userVar);
		}

		return true;
	}
	else if( responseLen_bytes != 0 )
	{
		OTA_LOG_WARN(TAG, "got malformed response");
	}

	// if we made it here, no FW update
	return false;
}


static void downloadUpdateWithUuid(char *targetUuidIn, size_t fwSize_bytesIn)
{
	if( (targetUuidIn == NULL) || (strlen(targetUuidIn) != UUID_LEN_BYTES) ) return;

	// figure out our block size and get ready for the OTA
	size_t blockSize_bytes;
	if( !ota_flash_otaSave_begin(&blockSize_bytes) )
	{
		OTA_LOG_ERROR(TAG, "flash reports not ready for OTA, aborting");
		return;
	}

	// calculate our total number of blocks
	size_t numBlocks = DIV_ROUND_UP(fwSize_bytesIn, blockSize_bytes);
	OTA_LOG_INFO(TAG, "starting download of %d bytes (%d blocks)", fwSize_bytesIn, numBlocks);

	char body[92];
	char response[blockSize_bytes+1];			// +1 for null-term
	uint16_t httpStatus;

	// gotta iterate our blocks here
	int numFailedTries = 0;
	size_t currBlockIndex = 0;
	while( currBlockIndex < numBlocks )
	{
		OTA_LOG_INFO(TAG, "downloading block %d / %d", currBlockIndex+1, numBlocks);

		// create our request body
		snprintf(body, sizeof(body), "{\"targetFwUuid\":\"%s\",\"offset\":%d,\"maxNumBytes\":%d}", targetUuidIn, currBlockIndex * blockSize_bytes, blockSize_bytes);
		body[sizeof(body)-1] = 0;

		// issue our request
		size_t numBytesInCurrBlock;
		if( ota_httpClient_postJson("/v1/getfwdata", body, &httpStatus, response, sizeof(response), &numBytesInCurrBlock, true) )
		{
			// if we made it here, we got a valid HTTP response...verify it
			if( httpStatus == 200 )
			{
				// got a valid response
				if( ota_flash_otaSave_writeBlock(response, numBytesInCurrBlock) )
				{
					currBlockIndex++;
				}
				else
				{
					OTA_LOG_ERROR(TAG, "flash failed to write block %d, aborting", currBlockIndex);
					ota_flash_otaSave_abort();
					ota_httpClient_closeConnection();
					return;
				}
			}
			else
			{
				OTA_LOG_WARN(TAG, "HTTP request did not return 200(OK), will retry");
				numFailedTries++;
			}
		}
		else
		{
			OTA_LOG_WARN(TAG, "failed to get block %d, will retry", currBlockIndex);
			numFailedTries++;
		}

		// make sure we don't try _too_ hard
		if( numFailedTries >= FW_DL_MAX_FAILED_TRIES )
		{
			OTA_LOG_ERROR(TAG, "exceeded max # of failures, aborting download");
			ota_httpClient_closeConnection();
			return;
		}

		// give us a bit of a delay between blocks so we don't cause much damage to other processes
		if( INTERBLOCK_DELAY_MS >0 ) ota_thread_delay_ms(INTERBLOCK_DELAY_MS);
	}

	// if we get to here, we're done receiving blocks
	ota_httpClient_closeConnection();
	if( ota_flash_otaSave_endAndValidate() )
	{
		OTA_LOG_INFO(TAG, "OTA update complete, rebooting");
		ota_thread_delay_ms(10000);
		ota_flash_reboot();
	}
	else
	{
		OTA_LOG_ERROR(TAG, "firmware validation failed");
	}
}
