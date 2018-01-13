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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "ota_cert_ca.h"
#include "ota_flash.h"
#include "ota_httpClient.h"
#include "ota_logging.h"
#include "ota_misc.h"
#include "ota_thread.h"
#include "ota_timeBase.h"


// ******** local macro definitions ********
#define OTA_HOSTNAME							"api.otaupdate.net"
#define OTA_PORTNUM							443
#define OTA_VERSION							"v1"

#define OTA_POLL_PERIOD_MS					5 * 60 * 1000
#define INTERBLOCK_DELAY_MS					0

#define UUID_LEN_BYTES						36
#define SERIALNUM_MAXLEN_BYTES				48
#define FWSIZESTR_MAXLEN_BYTES				9

#define FW_DL_MAX_FAILED_TRIES				8

#define TAG									"ota_updateClient"

#define DIV_ROUND_UP(num, denom)				(((num) + ((denom) - 1)) / (denom))


// ******** local type definitions ********
typedef enum
{
	CHECK_FOR_UPDATE_RETVAL_UP_TO_DATE,
	CHECK_FOR_UPDATE_RETVAL_UPDATE_AVAILABLE,
	CHECK_FOR_UPDATE_RETVAL_ERROR
}checkForUpdateRetVal_t;


// ******** local function prototypes ********
static checkForUpdateRetVal_t isUpdateAvailable(char* updateUuidOut, size_t *const fwSize_bytesOut);
static void downloadUpdateWithUuid(char *targetUuidIn, size_t fwSize_bytesIn);


// ********  local variable declarations *********
static char fwUuid[UUID_LEN_BYTES+1];
static char devSerialNum[SERIALNUM_MAXLEN_BYTES+1];

static bool isHttpClientInit = false;

static ota_updateClient_loggingFunction_t logFunction = NULL;
static void* logFunction_userVar = NULL;

static uint32_t lastCheckTime_us = 0;


// ******** global function implementations ********
bool ota_updateClient_init(const char *const fwUuidIn,
						   const char *const devSerialNumIn)
{
	// ensure we have our needed parameters
	if( fwUuidIn == NULL ) return false;
	if( devSerialNumIn == NULL ) return false;

	// store them locally
	if( !ota_copyStringToBufferSafely(fwUuidIn, fwUuid, sizeof(fwUuid)) ) return false;
	if( !ota_copyStringToBufferSafely(devSerialNumIn, devSerialNum, sizeof(devSerialNum)) ) return false;

	// get our start time
	lastCheckTime_us = ota_timeBase_getCount_us();

	// if we made it here, we were successful
	return true;
}


void ota_updateClient_iterate(void)
{
	uint32_t currTime_us = ota_timeBase_getCount_us();
	if( ((currTime_us - lastCheckTime_us) / 1000) < OTA_POLL_PERIOD_MS )
	{
		return;
	}
	lastCheckTime_us = currTime_us;

	// make sure out http client is initialized
	if( !isHttpClientInit )
	{
		// initialize our http client
		if( !ota_httpClient_init(OTA_HOSTNAME, OTA_PORTNUM, (const unsigned char*)OTA_CA_CERT, sizeof(OTA_CA_CERT)) )
		{
			OTA_LOG_ERROR(TAG, "error initializing http client, will retry next polling period");
			return;
		}
		isHttpClientInit = true;
	}

	// now we're free to check
	OTA_LOG_INFO(TAG, "checking for updates now");
	char targetUuid[UUID_LEN_BYTES+1];
	size_t firmwareSize_bytes;
	checkForUpdateRetVal_t updateRetVal = isUpdateAvailable(targetUuid, &firmwareSize_bytes);

	// print some brief information
	if( updateRetVal == CHECK_FOR_UPDATE_RETVAL_UPDATE_AVAILABLE )
	{
		OTA_LOG_INFO(TAG, "new FW available: '%s'", targetUuid);
	}
	else if( updateRetVal == CHECK_FOR_UPDATE_RETVAL_UP_TO_DATE )
	{
		OTA_LOG_INFO(TAG, "FW up-to-date");
	}

	// begin our update (if applicable)
	if( updateRetVal == CHECK_FOR_UPDATE_RETVAL_UPDATE_AVAILABLE )
	{
		downloadUpdateWithUuid(targetUuid, firmwareSize_bytes);
	}
}


void ota_updateClient_setLogFunction(ota_updateClient_loggingFunction_t loggingFunctionIn, void *userVarIn)
{
	logFunction = loggingFunctionIn;
	logFunction_userVar = userVarIn;
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
static checkForUpdateRetVal_t isUpdateAvailable(char* updateUuidOut, size_t *const fwSize_bytesOut)
{
	if( (updateUuidOut == NULL) || (fwSize_bytesOut == NULL) ) return CHECK_FOR_UPDATE_RETVAL_ERROR;

	// create our check-in request body...
	char body[128];
	snprintf(body, sizeof(body), "{\"currFwUuid\":\"%s\",\"procSerialNum\":\"%s\"}", fwUuid, devSerialNum);
	body[sizeof(body)-1] = 0;

	// send our check request
	uint16_t httpStatus;
	size_t responseLen_bytes;
	char responseBody[UUID_LEN_BYTES+1+FWSIZESTR_MAXLEN_BYTES+1];		// +1 for separating space, +1 for null-term
	if( !ota_httpClient_postJson("/" OTA_VERSION "/devs/checkforupdate", body, &httpStatus, responseBody, sizeof(responseBody), &responseLen_bytes, false) )
	{
		OTA_LOG_WARN(TAG, "update check failed, will retry later");
		return CHECK_FOR_UPDATE_RETVAL_ERROR;
	}

	// if we made it here, we got a valid HTTP response...verify it
	if( httpStatus != 200 )
	{
		OTA_LOG_WARN(TAG, "HTTP request did not return 200(OK) [%d], will retry later", httpStatus);
		return CHECK_FOR_UPDATE_RETVAL_ERROR;
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
			return CHECK_FOR_UPDATE_RETVAL_ERROR;
		}

		return CHECK_FOR_UPDATE_RETVAL_UPDATE_AVAILABLE;
	}
	else if( responseLen_bytes != 0 )
	{
		OTA_LOG_WARN(TAG, "got malformed response");
		return CHECK_FOR_UPDATE_RETVAL_ERROR;
	}

	// if we made it here, no FW update
	return CHECK_FOR_UPDATE_RETVAL_UP_TO_DATE;
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
		if( ota_httpClient_postJson("/" OTA_VERSION "/devs/getfwdata", body, &httpStatus, response, sizeof(response), &numBytesInCurrBlock, true) )
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
		if( INTERBLOCK_DELAY_MS > 0 ) ota_thread_delay_ms(INTERBLOCK_DELAY_MS);
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
