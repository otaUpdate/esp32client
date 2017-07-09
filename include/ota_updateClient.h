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


// ******** global macro definitions ********


// ******** global type definitions *********


// ******** global function prototypes ********
bool ota_updateClient_init(const char *const fwUuidIn, const char *const hwUuidIn, const char *const devSerialNumIn);

#endif
