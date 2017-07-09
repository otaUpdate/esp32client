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
#include "ota_httpClient.h"
#include "ota_logging.h"
#include "ota_misc.h"
#include "ota_thread.h"


// ******** local macro definitions ********
#define OTA_HOSTNAME						"devs.otaupdate.net"
#define OTA_PORTNUM							443

#define OTA_POLL_PERIOD_MS					10 * 1000

#define SERIALNUM_MAXLEN_BYTES				37

#define TAG									"ota_updateClient"


// ******** local type definitions ********


// ******** local function prototypes ********
static void mainThread(void);


// ********  local variable declarations *********
static char fwUuid[37];
static char hwUuid[37];
static char devSerialNum[SERIALNUM_MAXLEN_BYTES];


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


// ******** local function implementations ********
static void mainThread(void)
{
	// initialize our http client
	if( !ota_httpClient_init(OTA_HOSTNAME, OTA_PORTNUM, (const unsigned char*)OTA_CA_CERT, sizeof(OTA_CA_CERT)) ) return;

	while(1)
	{
		// wait for the next polling period
		OTA_LOG_DEBUG(TAG, "next update check in %d seconds", OTA_POLL_PERIOD_MS/1000);
		ota_thread_delay_ms(OTA_POLL_PERIOD_MS);
		OTA_LOG_INFO(TAG, "checking for updates now");

		// create our check-in request body...
		char body[57];
		snprintf(body, sizeof(body), "{\"currFwUuid\":\"%s\"}", fwUuid);
		body[sizeof(body)-1] = 0;

		// send out check-in request
		uint16_t httpStatus;
		char responseBody[37];
		if( !ota_httpClient_postJson("/v1/checkforupdate", body, &httpStatus, responseBody, sizeof(responseBody)) )
		{
			OTA_LOG_WARN(TAG, "update check failed...will retry");
			continue;
		}

		// if we made it here, we got a valid HTTP response...verify it
		if( httpStatus != 200 )
		{
			OTA_LOG_WARN(TAG, "HTTP request did not return 200(OK)...will retry");
			continue;
		}

		// if we made it here, update check was successful, see if we have an update
		if( strlen(responseBody) == 36 )
		{
			// got a new target fw UUID
			OTA_LOG_INFO(TAG, "new FW available: '%s'", responseBody);
			continue;
		}
		else
		{
			OTA_LOG_INFO(TAG, "FW up-to-date");
		}
	}
}
