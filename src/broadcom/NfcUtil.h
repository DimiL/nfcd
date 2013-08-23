/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Discovery modes -- keep in sync with NFCManager.DISCOVERY_MODE_* */
#define DISCOVERY_MODE_TAG_READER         0
#define DISCOVERY_MODE_NFCIP1             1
#define DISCOVERY_MODE_CARD_EMULATION     2
#define DISCOVERY_MODE_TABLE_SIZE         3

#define DISCOVERY_MODE_DISABLED           0
#define DISCOVERY_MODE_ENABLED            1

#define MODE_P2P_TARGET                   0
#define MODE_P2P_INITIATOR                1


/* Properties values */
#define PROPERTY_LLCP_LTO                 0
#define PROPERTY_LLCP_MIU                 1
#define PROPERTY_LLCP_WKS                 2
#define PROPERTY_LLCP_OPT                 3
#define PROPERTY_NFC_DISCOVERY_A          4
#define PROPERTY_NFC_DISCOVERY_B          5
#define PROPERTY_NFC_DISCOVERY_F          6
#define PROPERTY_NFC_DISCOVERY_15693      7
#define PROPERTY_NFC_DISCOVERY_NCFIP      8


/* Error codes */
#define ERROR_BUFFER_TOO_SMALL            -12
#define ERROR_INSUFFICIENT_RESOURCES      -9


/* Pre-defined tag type values. These must match the values in
 * Ndef.java in the framework.
 */
#define NDEF_UNKNOWN_TYPE                -1
#define NDEF_TYPE1_TAG                   1
#define NDEF_TYPE2_TAG                   2
#define NDEF_TYPE3_TAG                   3
#define NDEF_TYPE4_TAG                   4
#define NDEF_MIFARE_CLASSIC_TAG          101


/* Pre-defined card read/write state values. These must match the values in
 * Ndef.java in the framework.
 */
#define NDEF_MODE_READ_ONLY              1
#define NDEF_MODE_READ_WRITE             2
#define NDEF_MODE_UNKNOWN                3


/* Name strings for target types. These *must* match the values in TagTechnology.java */
#define TARGET_TYPE_UNKNOWN               -1
#define TARGET_TYPE_ISO14443_3A           1
#define TARGET_TYPE_ISO14443_3B           2
#define TARGET_TYPE_ISO14443_4            3
#define TARGET_TYPE_FELICA                4
#define TARGET_TYPE_ISO15693              5
#define TARGET_TYPE_NDEF                  6
#define TARGET_TYPE_NDEF_FORMATABLE       7
#define TARGET_TYPE_MIFARE_CLASSIC        8
#define TARGET_TYPE_MIFARE_UL             9
#define TARGET_TYPE_KOVIO_BARCODE         10


//define a few NXP error codes that NFC service expects;
//see external/libnfc-nxp/src/phLibNfcStatus.h;
//see external/libnfc-nxp/inc/phNfcStatus.h
#define NFCSTATUS_SUCCESS (0x0000)
#define NFCSTATUS_FAILED (0x00FF)

//default general trasceive timeout in millisecond
#define DEFAULT_GENERAL_TRANS_TIMEOUT  1000
