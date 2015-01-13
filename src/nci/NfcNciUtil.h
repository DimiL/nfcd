/*
 * Copyright (C) 2014  Mozilla Foundation
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

#pragma once

#include <pthread.h>
#include "TagTechnology.h"

/**
 * Discovery modes -- keep in sync with NFCManager.DISCOVERY_MODE_*
 */
#define DISCOVERY_MODE_TAG_READER         0
#define DISCOVERY_MODE_NFCIP1             1
#define DISCOVERY_MODE_CARD_EMULATION     2
#define DISCOVERY_MODE_TABLE_SIZE         3

#define DISCOVERY_MODE_DISABLED           0
#define DISCOVERY_MODE_ENABLED            1

//#define MODE_P2P_TARGET                   0
//#define MODE_P2P_INITIATOR                1

/**
 * Properties values.
 */
#define PROPERTY_LLCP_LTO                 0
#define PROPERTY_LLCP_MIU                 1
#define PROPERTY_LLCP_WKS                 2
#define PROPERTY_LLCP_OPT                 3
#define PROPERTY_NFC_DISCOVERY_A          4
#define PROPERTY_NFC_DISCOVERY_B          5
#define PROPERTY_NFC_DISCOVERY_F          6
#define PROPERTY_NFC_DISCOVERY_15693      7
#define PROPERTY_NFC_DISCOVERY_NCFIP      8

/**
 * Error codes.
 */
#define ERROR_BUFFER_TOO_SMALL            -12
#define ERROR_INSUFFICIENT_RESOURCES      -9

/**
 * Pre-defined card read/write state values. These must match the values in
 * Ndef.java in the framework.
 */
#define NDEF_MODE_READ_ONLY              1
#define NDEF_MODE_READ_WRITE             2
#define NDEF_MODE_UNKNOWN                3


/**
 * Name strings for target types.
 */
#define TARGET_TYPE_UNKNOWN               -1
#define TARGET_TYPE_ISO14443_3A           1
#define TARGET_TYPE_ISO14443_3B           2
#define TARGET_TYPE_ISO14443_4            3
#define TARGET_TYPE_FELICA                4
#define TARGET_TYPE_ISO15693              5
#define TARGET_TYPE_MIFARE_CLASSIC        6
#define TARGET_TYPE_MIFARE_UL             7
#define TARGET_TYPE_KOVIO_BARCODE         8


// Define a few NXP error codes that NFC service expects.
// See external/libnfc-nxp/src/phLibNfcStatus.h.
// See external/libnfc-nxp/inc/phNfcStatus.h.
#define NFCSTATUS_SUCCESS (0x0000)
#define NFCSTATUS_FAILED (0x00FF)

// Default general trasceive timeout in millisecond.
#define DEFAULT_GENERAL_TRANS_TIMEOUT  1000

struct nfc_data
{
  // Thread handle.
  pthread_t thread;
  int running;

  // Reference to the NFCManager instance.
  void* manager;

  // Secure Element selected.
  int seId;

  // LLCP params.
  int lto;
  int miu;
  int wks;
  int opt;

  int tech_mask;

  // Tag detected.
  void* tag;

  int tHandle;
  int tProtocols[16];
  int handles[16];
};

class NfcNciUtil {
public:
  static TagTechnology toGenericTagTechnology(unsigned int tagTech);
  static unsigned int toNciTagTechnology(TagTechnology tagTech);
private:
  NfcNciUtil();
};
