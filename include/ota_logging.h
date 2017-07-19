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
#include "ota_updateClient.h"
#include "sdkconfig.h"


// ******** global macro definitions ********
#define OTA_LOG_LEVEL_NONE			0
#define OTA_LOG_LEVEL_ERROR			1
#define OTA_LOG_LEVEL_WARN			2
#define OTA_LOG_LEVEL_INFO			3
#define OTA_LOG_LEVEL_DEBUG			4

#ifdef CONFIG_OTA_LOG_LEVEL
#define OTA_LOG_LEVEL				CONFIG_OTA_LOG_LEVEL
#endif

#if( (!defined OTA_LOG_LEVEL) || (OTA_LOG_LEVEL == OTA_LOG_LEVEL_NONE) )
#define OTA_LOG_ERROR(tagIn, fmtIn, ...)
#define OTA_LOG_WARN(tagIn, fmtIn, ...)
#define OTA_LOG_INFO(tagIn, fmtIn, ...)
#define OTA_LOG_DEBUG(tagIn, fmtIn, ...)
#elif( (defined OTA_LOG_LEVEL) && (OTA_LOG_LEVEL == OTA_LOG_LEVEL_ERROR) )
#define OTA_LOG_ERROR(tagIn, fmtIn, ...)		ota_updateClient_log(OTA_LOG_LEVEL_ERROR, tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_WARN(tagIn, fmtIn, ...)
#define OTA_LOG_INFO(tagIn, fmtIn, ...)
#define OTA_LOG_DEBUG(tagIn, fmtIn, ...)
#elif( (defined OTA_LOG_LEVEL) && (OTA_LOG_LEVEL == OTA_LOG_LEVEL_WARN) )
#define OTA_LOG_ERROR(tagIn, fmtIn, ...)		ota_updateClient_log(OTA_LOG_LEVEL_ERROR, tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_WARN(tagIn, fmtIn, ...)			ota_updateClient_log(OTA_LOG_LEVEL_WARN,  tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_INFO(tagIn, fmtIn, ...)
#define OTA_LOG_DEBUG(tagIn, fmtIn, ...)
#elif( (defined OTA_LOG_LEVEL) && (OTA_LOG_LEVEL == OTA_LOG_LEVEL_INFO) )
#define OTA_LOG_ERROR(tagIn, fmtIn, ...)		ota_updateClient_log(OTA_LOG_LEVEL_ERROR, tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_WARN(tagIn, fmtIn, ...)			ota_updateClient_log(OTA_LOG_LEVEL_WARN,  tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_INFO(tagIn, fmtIn, ...)			ota_updateClient_log(OTA_LOG_LEVEL_INFO,  tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_DEBUG(tagIn, fmtIn, ...)
#elif( (defined OTA_LOG_LEVEL) && (OTA_LOG_LEVEL == OTA_LOG_LEVEL_DEBUG) )
#define OTA_LOG_ERROR(tagIn, fmtIn, ...)		ota_updateClient_log(OTA_LOG_LEVEL_ERROR, tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_WARN(tagIn, fmtIn, ...)			ota_updateClient_log(OTA_LOG_LEVEL_WARN,  tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_INFO(tagIn, fmtIn, ...)			ota_updateClient_log(OTA_LOG_LEVEL_INFO,  tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#define OTA_LOG_DEBUG(tagIn, fmtIn, ...)		ota_updateClient_log(OTA_LOG_LEVEL_DEBUG, tagIn, tagIn " " fmtIn, ##__VA_ARGS__)
#endif



// ******** global type definitions *********


// ******** global function prototypes ********


#endif
