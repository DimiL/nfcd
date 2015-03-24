/*
 * Copyright (C) 2013-2014  Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_nfcd_NfcDebug_h
#define mozilla_nfcd_NfcDebug_h

#include "utils/Log.h"

extern bool gNfcDebugFlag;

#define FUNC __PRETTY_FUNCTION__

// Property to enable/disable daemon log
// It is only read when nfcd starts.
#define NFC_DEBUG_PROPERTY "debug.nfcd.enabled"

#define TAG_NFCD "nfcd"
#define TAG_NCI "NfcNci"

#define NFC_DEBUG(level, tag, msg, ...)                                  \
  if (gNfcDebugFlag) {                                                   \
    __android_log_print(level, tag, "%s: " msg, FUNC, ##__VA_ARGS__);    \
  }

#define NFCD_DEBUG(msg, ...)  \
  NFC_DEBUG(ANDROID_LOG_DEBUG, TAG_NFCD, msg, ##__VA_ARGS__)
#define NFCD_WARNING(msg, ...)  \
  NFC_DEBUG(ANDROID_LOG_WARN, TAG_NFCD, msg, ##__VA_ARGS__)
#define NFCD_ERROR(msg, ...)  \
  NFC_DEBUG(ANDROID_LOG_ERROR, TAG_NFCD, msg, ##__VA_ARGS__)

#define NCI_DEBUG(msg, ...)  \
  NFC_DEBUG(ANDROID_LOG_DEBUG, TAG_NCI, msg, ##__VA_ARGS__)
#define NCI_WARNING(msg, ...)  \
  NFC_DEBUG(ANDROID_LOG_WARN, TAG_NCI, msg, ##__VA_ARGS__)
#define NCI_ERROR(msg, ...)  \
  NFC_DEBUG(ANDROID_LOG_ERROR, TAG_NCI, msg, ##__VA_ARGS__)

#endif
