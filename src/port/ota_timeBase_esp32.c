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
#include "ota_timeBase.h"


#include <math.h>
#include <stdbool.h>
#include <sys/time.h>


// ******** local macro definitions ********


// ******** local type definitions ********


// ******** local function prototypes ********


// ********  local variable declarations *********
static bool hasInitVal = false;
static struct timeval initVal = {0, 0};


// ******** global function implementations ********
uint32_t ota_timeBase_getCount_us(void)
{
	if( !hasInitVal )
	{
		gettimeofday(&initVal, NULL);
		hasInitVal = true;
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);

	// now subtract to compare
	struct timeval diff;

	if( initVal.tv_usec > tv.tv_usec )
	{
		tv.tv_sec--;
		tv.tv_usec += 1E6;
	}

	diff.tv_sec = tv.tv_sec - initVal.tv_sec;
	diff.tv_usec = tv.tv_usec - initVal.tv_usec;

	return fmod((diff.tv_sec * 1E6 + diff.tv_usec), UINT32_MAX);
}


// ******** local function implementations ********
