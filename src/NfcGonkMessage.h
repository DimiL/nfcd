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

#ifndef NFC_GONK_MESSAGE_H
#define NFC_GONK_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NFC parcel format.
 *
 * NFC Request:
 *    4 bytes of parcel size. (Big-Endian)
 *    4 byte of request type. Value will be one of NfcRequestType.
 *    (Parcel size - 4) bytes of request data.
 *
 * NFC Response:
 *    4 bytes of parcel size. (Big-Endian)
 *    4 byte of response type.
 *    4 bytes of error code. Value will be one of NfcErrorCode.
 *    (Parcel size - 8) bytes of response data.
 *
 * NFC Notification:
 *    4 bytes of parcel size. (Big-endian)
 *    4 bytes of notification type. Value will one of NfcNotificationType.
 *    (Parcel size - 4) bytes of notification data.
 *
 * Except Parcel size is encoded in Big-Endian, other data will be encoded in
 * Little-Endian.
 */

/**
 * Message types sent from NFCC (NFC Controller)
 */
typedef enum {
  NFCC_MESSAGE_RESPONSE = 0,
  NFCC_MESSAGE_NOTIFICATION = 1
} NFCCMessageType;

/**
 * Error code.
 */
typedef enum {
  NFC_SUCCESS = 0,
  NFC_ERROR_IO = -1,
  NFC_ERROR_CANCELLED = -2,
  NFC_ERROR_TIMEOUT = -3,
  NFC_ERROR_BUSY = -4,
  NFC_ERROR_CONNECT = -5,
  NFC_ERROR_DISCONNECT = -6,
  NFC_ERROR_READ = -7,
  NFC_ERROR_WRITE = -8,
  NFC_ERROR_INVALID_PARAM = -9,
  NFC_ERROR_INSUFFICIENT_RESOURCES = -10,
  NFC_ERROR_SOCKET_CREATION = -11,
  NFC_ERROR_SOCKET_NOT_CONNECTED = -12,
  NFC_ERROR_BUFFER_TOO_SMALL = -13,
  NFC_ERROR_SAP_USED = -14,
  NFC_ERROR_SERVICE_NAME_USED = -15,
  NFC_ERROR_SOCKET_OPTIONS = -16,
  NFC_ERROR_FAIL_ENABLE_DISCOVERY = -17,
  NFC_ERROR_FAIL_DISABLE_DISCOVERY = -18,
  NFC_ERROR_NOT_INITIALIZED = -19,
  NFC_ERROR_INITIALIZE_FAIL = -20,
  NFC_ERROR_DEINITIALIZE_FAIL = -21,
  NFC_ERROR_SE_CONNECTED = -22,
  NFC_ERROR_NO_SE_CONNECTED = -23,
  NFC_ERROR_NOT_SUPPORTED = -24,
  NFC_ERROR_BAD_SESSION_ID = -25,
  NFC_ERROR_LOST_TECH = -26,
  NFC_ERROR_BAD_TECH_TYPE = -27,
  NFC_ERROR_SELECT_SE_FAIL = -28,
  NFC_ERROR_DESELECT_SE_FAIL = -29,
  NFC_ERROR_FAIL_ENABLE_LOW_POWER_MODE = -30,
  NFC_ERROR_FAIL_DISABLE_LOW_POWER_MODE = -31,
} NfcErrorCode;

/**
 * Power saving mode.
 */
typedef enum {
  /**
   * No action, a no-op to be able to mimic optional parameters once
   * additional config parameters will be introduced.
   */
  NFC_POWER_NO_OP = -1,

  /**
   * Turn off NFC chip
   */
  NFC_POWER_OFF = 0,

  /**
   * Request NFC chip to goto low-power mode.
   */
  NFC_POWER_LOW = 1,

  /**
   * Request NFC chip to goto full-power mode.
   */
  NFC_POWER_FULL = 2,
} NfcPowerLevel;

/**
 * NFC technologies.
 */
typedef enum {
  NFC_TECH_UNKNOWN = -1,
  NFC_TECH_NDEF = 0,
  NFC_TECH_NDEF_WRITABLE = 1,
  NFC_TECH_NDEF_FORMATABLE = 2,
  NFC_TECH_P2P = 3,
  NFC_TECH_NFCA = 4,
  NFC_TECH_NFCB = 5,
  NFC_TECH_NFCF = 6,
  NFC_TECH_NFCV = 7,
  NFC_TECH_ISO_DEP = 8,
  NFC_TECH_MIFARE_CLASSIC = 9,
  NFC_TECH_MIFARE_ULTRALIGHT = 10,
  NFC_TECH_BARCODE = 11
} NfcTechnology;

/**
 * NDEF type
 */
typedef enum {
  NFC_NDEF_UNKNOWN_TAG = -1,
  NFC_NDEF_TYPE_1_TAG = 0,
  NFC_NDEF_TYPE_2_TAG = 1,
  NFC_NDEF_TYPE_3_TAG = 2,
  NFC_NDEF_TYPE_4_TAG = 3,
  NFC_NDEF_MIFARE_CLASSIC_TAG = 4,
} NfcNdefType;

/**
 * NDEF Record
 * @see NFCForum-TS-NDEF, clause 3.2
 */
typedef struct {
  uint32_t tnf;

  uint32_t typeLength;
  uint8_t* type;

  uint32_t idLength;
  uint8_t* id;

  uint32_t payloadLength;
  uint8_t* payload;
} NdefRecordPdu;

/**
 * NDEF Message.
 */
typedef struct {
  uint32_t numRecords;
  NdefRecordPdu* records;
} NdefMessagePdu;

/**
 * Session Id.
 */
typedef uint32_t NfcSessionId;

typedef struct {
  NfcPowerLevel powerLevel;
} NfcConfigRequest;

typedef struct {
  /**
   * possible values are : TODO
   */
  uint32_t status;
} NfcConfigResponse;

typedef struct {
  /**
   * The sessionId must correspond to that of a prior
   * NfcNotificationTechDiscovered.
   */
  NfcSessionId sessionId;

  uint8_t technology;
} NfcConnectRequest;

typedef struct {
  /**
   * The sessionId must correspond to that of a prior
   * NfcNotificationTechDiscovered.
   */
  NfcSessionId sessionId;

  //TODO Parcel doesn't have API for boolean, should we use bit-wise for this?
  /**
   * The NDEF is read-only or not.
   */
  uint8_t isReadOnly;

  /**
   * The NDEF can be configured to read-only or not.
   */
  uint8_t canBeMadeReadonly;

  /**
   * Maximum length of the NDEF.
   */
  uint32_t maxNdefLength;
} NfcGetDetailsResponse;

typedef struct {
  /**
   * The sessionId must correspond to that of a prior
   * NfcNotificationTechDiscovered.
   */
  NfcSessionId sessionId;

  /**
   * NDEF Message to be transmitted.
   */
  NdefMessagePdu ndef;
} NfcNdefReadWritePdu;

typedef enum {
  /**
   * NFC_REQUEST_CONFIG
   *
   * Config NFCD options.
   *
   * data is NfcConfigRequest.
   *
   * response is NfcConfigResponse.
   */
  NFC_REQUEST_CONFIG = 0,

  /**
   * NFC_REQUEST_CONNECT
   *
   * Connect to a specific NFC-compatible technology.
   *
   * data is NfcConnectRequest.
   *
   * response is NULL.
   */
  NFC_REQUEST_CONNECT = 1,

  /**
   * NFC_REQUEST_CLOSE
   *
   * Close an open connection that must have been opened with a prior
   * NfcConnectRequest.
   *
   * data is NfcSessionId, which is correlates to a technology that was
   * previously discovered with NFC_NOTIFICATION_TECH_DISCOVERED.
   *
   * response is NULL.
   */
  NFC_REQUEST_CLOSE = 2,

  /**
   * NFC_REQUEST_GET_DETAILS
   *
   * Request the NDEF meta-data. The 'technology' field in
   * NfcNotificationTechDiscovered must include NFC_TECH_NDEF.
   *
   * data is NfcSessionId, which is correlates to a technology that was
   * previously discovered with NFC_NOTIFICATION_TECH_DISCOVERED.
   *
   * respose is NfcGetDetailsResponse.
   */
  NFC_REQUEST_GET_DETAILS = 3,

  /**
   * NFC_REQUEST_READ_NDEF
   *
   * Request the scanned NDEF message. The 'technology' field in
   * NfcNotificationTechDiscovered must include NFC_TECH_NDEF.
   *
   * data is NfcSessionId, which is correlates to a technology that was
   * previously discovered with NFC_NOTIFICATION_TECH_DISCOVERED.
   *
   * response is NfcNdefReadWritePdu.
   */
  NFC_REQUEST_READ_NDEF = 4,

  /**
   * NFC_REQUEST_WRITE_NDEF
   *
   * Write a NDEF message. The 'technology' field in
   * NfcNotificationTechDiscovered must include NFC_TECH_NDEF_WRITABLE or
   * NFC_TECH_P2P.
   *
   * data is NfcNdefReadWritePdu.
   *
   * response is NULL.
   */
  NFC_REQUEST_WRITE_NDEF = 5,

  /**
   * NFC_REQUEST_MAKE_NDEF_READ_ONLY
   *
   * Make the NDEF message is read-only. The 'technology' field in
   * NfcNotificationTechDiscovered must include NFC_TECH_NDEF_WRITABLE.
   *
   * data is NfcSessionId, which is correlates to a technology that was
   * previously discovered with NFC_NOTIFICATION_TECH_DISCOVERED.
   *
   * response is NULL.
   */
  NFC_REQUEST_MAKE_NDEF_READ_ONLY = 6,
} NfcRequestType;

typedef enum {
  NFC_RESPONSE_GENERAL = 1000,

  NFC_RESPONSE_CONFIG = 1001,

  NFC_RESPONSE_READ_NDEF_DETAILS = 1002,

  NFC_RESPONSE_READ_NDEF = 1003,
} NfcResponseType;

typedef struct {
  uint32_t status;
  uint32_t majorVersion;
  uint32_t minorVersion;
} NfcNotificationInitialized;

typedef struct {
  NfcSessionId sessionId;
  uint32_t numOfTechnogies;
  uint8_t* technology;

  uint32_t numOfNdefMsgs;
  NdefMessagePdu* ndef;
} NfcNotificationTechDiscovered;

typedef enum {
  NFC_NOTIFICATION_BASE = 1999,

  /**
   * NFC_NOTIFICATION_INITIALIZED
   *
   * To notify nfcd is initialized after startup.
   *
   * data is NfcNotificationInitialized.
   */
  NFC_NOTIFICATION_INITIALIZED = 2000,

  /**
   * NFC_NOTIFICATION_TECH_DISCOVERED
   *
   * To notify a NFC-compatible technology has been discovered.
   *
   * data is NfcNotificationTechDiscovered.
   */
  NFC_NOTIFICATION_TECH_DISCOVERED = 2001,

  /**
   * NFC_NOTIFICATION_TECH_LOST
   *
   * To notify whenever a NFC-compatible technology is removed from the field of
   * the NFC reader.
   *
   * data is char* sessionId, which is correlates to a technology that was
   * previously discovered with NFC_NOTIFICATION_TECH_DISCOVERED.
   */
  NFC_NOTIFICATION_TECH_LOST = 2002,

  /**
   * NFC_NOTIFICATION_TRANSACTION_EVENT
   *
   * To notify a transaction event from secure element.
   *
   * data is [origin type][origin index][aid length][aid][payload length][payload]
   */
  NFC_NOTIFICATION_TRANSACTION_EVENT = 2003,
} NfcNotificationType;

/**
 * The origin of the transaction event.
 */
typedef enum {
  NFC_EVT_TRANSACTION_SIM = 0,

  NFC_EVT_TRANSACTION_ESE = 1,

  NFC_EVT_TRANSACTION_ASSD = 2,
} NfcEvtTransactionOrigin;

#ifdef __cplusplus
}
#endif

#endif // NFC_GONK_MESSAGE_H
