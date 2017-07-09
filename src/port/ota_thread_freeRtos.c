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
#include "ota_thread.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// ******** local macro definitions ********


// ******** local type definitions ********


// ******** local function prototypes ********
static void thread_freeRtosTask(void *pvParameters);


// ********  local variable declarations *********


// ******** global function implementations ********
void ota_thread_startThreadWithCallback(ota_thread_cb_t cbIn)
{
	xTaskCreate(thread_freeRtosTask, (const char * const)"otaUpdate", 8192, (void*)cbIn, tskIDLE_PRIORITY, NULL);
}


void ota_thread_delay_ms(uint32_t delay_msIn)
{
	vTaskDelay(delay_msIn / portTICK_PERIOD_MS);
}


// ******** local function implementations ********
static void thread_freeRtosTask(void *pvParameters)
{
	if( pvParameters == NULL ) return;

	ota_thread_cb_t cb = (ota_thread_cb_t)pvParameters;
	cb();

	// we _shouldn't_ get to here, but just in case
	while(1) taskYIELD();
}
