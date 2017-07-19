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
#ifndef OTA_FLASH_H_
#define OTA_FLASH_H_


// ******** includes ********
#include <stdbool.h>
#include <stdlib.h>


// ******** global macro definitions ********


// ******** global type definitions *********


// ******** global function prototypes *******
void ota_flash_printPartitionInfo(void);

bool ota_flash_otaSave_begin(size_t *const requestedBlockSize_bytesOut);
bool ota_flash_otaSave_writeBlock(const void* dataIn, size_t numBytesIn);
bool ota_flash_otaSave_endAndValidate(void);
void ota_flash_otaSave_abort(void);

void ota_flash_reboot(void);

#endif
