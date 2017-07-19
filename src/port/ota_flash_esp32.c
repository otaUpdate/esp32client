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
#include "ota_flash.h"


#include "esp_ota_ops.h"
#include "esp_system.h"
#include "ota_logging.h"


// ******** local macro definitions ********
#define TAG									"ota_flash"


// ******** local type definitions ********
#define BLOCKSIZE_BYTES							512


// ******** local function prototypes ********


// ********  local variable declarations *********
static bool isOtaInProgress = false;
static esp_ota_handle_t otaHandle;
static const esp_partition_t* targetPartition = NULL;


// ******** global function implementations ********
void ota_flash_printPartitionInfo(void)
{
	const esp_partition_t* runningPartition = esp_ota_get_running_partition();
	if( runningPartition != NULL )
	{
		OTA_LOG_INFO(TAG, "running partition '%s'", runningPartition->label);
	}
	else
	{
		OTA_LOG_WARN(TAG, "error determining running partition");
	}
}


bool ota_flash_otaSave_begin(size_t *const requestedBlockSize_bytesOut)
{
	if( requestedBlockSize_bytesOut == NULL ) return false;
	*requestedBlockSize_bytesOut = BLOCKSIZE_BYTES;

	// reset our flags
	isOtaInProgress = false;
	targetPartition = NULL;

	// try to figure out which partition we'll be using
	targetPartition = esp_ota_get_next_update_partition(NULL);
	if( targetPartition == NULL) return false;
	OTA_LOG_INFO(TAG, "downloading new OTA to partition '%s'", targetPartition->label);

	// begin the process
	esp_err_t err;
	if( (err = esp_ota_begin(targetPartition, OTA_SIZE_UNKNOWN, &otaHandle)) != ESP_OK )
	{
		OTA_LOG_ERROR(TAG, "error calling esp_ota_begin: %d", err);
		return false;
	}

	// if we made it here, we've started successfully
	isOtaInProgress = true;
	return true;
}


bool ota_flash_otaSave_writeBlock(const void* dataIn, size_t numBytesIn)
{
	if( !isOtaInProgress ) return false;

	// try to write the bytes
	OTA_LOG_DEBUG(TAG, "writing block of %d bytes", numBytesIn);
	esp_err_t err;
	if( (err = esp_ota_write(otaHandle, dataIn, numBytesIn)) != ESP_OK )
	{
		OTA_LOG_ERROR(TAG, "error writing block: %d", err);

		// free any memory associated with this OTA handle
		ota_flash_otaSave_abort();

		return false;
	}

	// if we made it here, we've written the block
	return true;
}


bool ota_flash_otaSave_endAndValidate(void)
{
	if( !isOtaInProgress ) return false;

	// end and validate
	esp_err_t err;
	if( (err = esp_ota_end(otaHandle)) != ESP_OK )
	{
		OTA_LOG_ERROR(TAG, "error calling esp_ota_end: %d", err);

		isOtaInProgress = false;
		targetPartition = NULL;
		return false;
	}

	// if we made it here, we ended successfully...switch the boot partition
	OTA_LOG_DEBUG(TAG, "setting boot partition to '%s'", targetPartition->label);
	err = esp_ota_set_boot_partition(targetPartition);
	isOtaInProgress = false;
	targetPartition = NULL;

	if( err != ESP_OK ) OTA_LOG_ERROR(TAG, "error setting boot partition: %d", err);
	return (err == ESP_OK);
}


void ota_flash_otaSave_abort(void)
{
	OTA_LOG_WARN(TAG, "abort requested");
	esp_ota_end(otaHandle);

	isOtaInProgress = false;
	targetPartition = NULL;
}


void ota_flash_reboot(void)
{
	esp_restart();
}


// ******** local function implementations ********
