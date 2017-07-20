/**
 * @file
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
#ifndef OTA_UPDATECLIENT_H_
#define OTA_UPDATECLIENT_H_


// ******** includes ********
#include <stdbool.h>
#include <stdarg.h>


// ******** global macro definitions ********
#ifndef OTA_UPDATECLIENT_MAXNUM_CBS
	#define OTA_UPDATECLIENT_MAXNUM_CBS			2
#endif


// ******** global type definitions *********
typedef void (*ota_updateClient_loggingFunction_t)(void *userVarIn, int otaLogLevelIn, const char *const tagIn, const char *const fmtIn, va_list argsIn);
typedef void (*ota_updateClient_updateAvailableCb_t)(const char *const updateUuidIn, void *const userVarIn);
typedef void (*ota_updateClient_willUpdateCb_t)(void *const userVarIn);


// ******** global function prototypes ********
bool ota_updateClient_init(const char *const fwUuidIn, const char *const devSerialNumIn);

void ota_updateClient_setLogFunction(ota_updateClient_loggingFunction_t loggingFunctionIn, void *userVarIn);

bool ota_updateClient_addListener(ota_updateClient_updateAvailableCb_t cb_updateAvailableIn,
								  ota_updateClient_willUpdateCb_t cb_willUpdateIn,
								  void* userVarIn);

/**
 * @protected
 */
void ota_updateClient_log(int otaLogLevelIn, const char *const tagIn, const char *const fmtIn, ...);


#endif
