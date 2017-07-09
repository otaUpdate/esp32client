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
#ifndef OTA_HTTPCLIENT_H_
#define OTA_HTTPCLIENT_H_


// ******** includes ********
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


// ******** global macro definitions ********


// ******** global type definitions *********


// ******** global function prototypes ********
bool ota_httpClient_init(const char *const hostnameIn, const uint16_t portNumIn, const unsigned char *rootCaCertIn, size_t rootCaCertSize_bytesIn);

bool ota_httpClient_postJson(const char *const urlIn, const char *const bodyIn,
							 uint16_t *const statusOut, char *const bodyOut, const size_t bodyOutMaxSize_bytesIn);

#endif
