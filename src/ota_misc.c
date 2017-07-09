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
#include <stdbool.h>
#include <stdlib.h>


// ******** local macro definitions ********


// ******** local type definitions ********


// ******** local function prototypes ********


// ********  local variable declarations *********


// ******** global function implementations ********
bool ota_copyStringToBufferSafely(const char *const srcIn, char* destIn, size_t maxDestBufferSize_bytesIn)
{
	for( size_t i = 0; i < maxDestBufferSize_bytesIn; i++ )
	{
		destIn[i] = srcIn[i];
		if( destIn[i] == 0 ) return true;
	}

	// if we made it here, we did not find a null-term within our target's maximum size
	return false;
}


// ******** local function implementations ********

