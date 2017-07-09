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
#ifndef OTA_LOGGING_H_
#define OTA_LOGGING_H_


// ******** includes ********


// ******** global macro definitions ********
#define OTA_LOG_ERROR(tagIn, fmtIn, ...)		printf("E %s " fmtIn "\r\n", tagIn, ##__VA_ARGS__); fflush(stdout);
#define OTA_LOG_WARN(tagIn, fmtIn, ...)			printf("W %s " fmtIn "\r\n", tagIn, ##__VA_ARGS__); fflush(stdout);
#define OTA_LOG_INFO(tagIn, fmtIn, ...)			printf("I %s " fmtIn "\r\n", tagIn, ##__VA_ARGS__); fflush(stdout);
#define OTA_LOG_DEBUG(tagIn, fmtIn, ...)		printf("D %s " fmtIn "\r\n", tagIn, ##__VA_ARGS__); fflush(stdout);


// ******** global type definitions *********


// ******** global function prototypes ********


#endif
