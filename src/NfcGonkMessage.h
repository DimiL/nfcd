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
  NFC_ERROR_IO = 1,
  NFC_ERROR_TIMEOUT = 2,
  NFC_ERROR_BUSY = 3,
  NFC_ERROR_CONNECT = 4,
  NFC_ERROR_DISCONNECT = 5,
  NFC_ERROR_READ = 6,
  NFC_ERROR_WRITE = 7,
  NFC_ERROR_INVALID_PARAM = 8,
  NFC_ERROR_INSUFFICIENT_RESOURCES = 9,
  NFC_ERROR_SOCKET_CREATION = 10,
  NFC_ERROR_FAIL_ENABLE_DISCOVERY = 11,
  NFC_ERROR_FAIL_DISABLE_DISCOVERY = 12,
  NFC_ERROR_NOT_INITIALIZED = 13,
  NFC_ERROR_INITIALIZE_FAIL = 14,
  NFC_ERROR_DEINITIALIZE_FAIL = 15,
  NFC_ERROR_NOT_SUPPORTED = 16,
  NFC_ERROR_FAIL_ENABLE_LOW_POWER_MODE = 17,
  NFC_ERROR_FAIL_DISABLE_LOW_POWER_MODE = 18,
} NfcErrorCode;

/**
 * Power saving mode.
 */
typedef enum {
  /**
   * No action, a no-op to be able to mimic optional parameters once
   * additional config parameters will be introduced.
   */
  NFC_RF_STATE_NO_OP = -1,

  /**
   * Enter IDLE mode.
   */
  NFC_RF_STATE_IDLE = 0,

  /**
   * Enter Listen mode.
   */
  NFC_RF_STATE_LISTEN = 1,

  /**
   * Enter Discovery mode.
   */
  NFC_RF_STATE_DISCOVERY = 2,
} NfcRFState;

/**
 * NFC technologies.
 */
typedef enum {
  NFC_TECH_UNKNOWN = -1,
  NFC_TECH_NFCA = 0,
  NFC_TECH_NFCB = 1,
  NFC_TECH_NFCF = 2,
  NFC_TECH_NFCV = 3,
  NFC_TECH_ISO_DEP = 4,
  NFC_TECH_MIFARE_CLASSIC = 5,
  NFC_TECH_MIFARE_ULTRALIGHT = 6,
  NFC_TECH_BARCODE = 7
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
  NfcRFState rfState;
} NfcChangeRFStateRequest;

typedef struct {
  /**
   * possible values are : TODO
   */
  uint32_t status;
} NfcChangeRFStateResponse;

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

  /**
   * NDEF Message to be transmitted.
   */
  NdefMessagePdu ndef;
} NfcNdefReadWritePdu;

typedef enum {
  /**
   * NFC_REQUEST_CHANGE_RF_STATE
   *
   * Change RF State.
   *
   * data is NfcChangeRFStateRequest.
   *
   * response is NfcChangeRFStateResponse.
   */
  NFC_REQUEST_CHANGE_RF_STATE = 0,

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
  NFC_REQUEST_READ_NDEF = 3,

  /**
   * NFC_REQUEST_WRITE_NDEF
   *
   * Write a NDEF message. The 'technology' field in
   * NfcNotificationTechDiscovered must be a p2p connection or
   * it is a writable tag.
   *
   * data is NfcNdefReadWritePdu.
   *
   * response is NULL.
   */
  NFC_REQUEST_WRITE_NDEF = 4,

  /**
   * NFC_REQUEST_MAKE_NDEF_READ_ONLY
   *
   * Make the NDEF message is read-only. The tag must be writable.
   *
   * data is NfcSessionId, which is correlates to a technology that was
   * previously discovered with NFC_NOTIFICATION_TECH_DISCOVERED.
   *
   * response is NULL.
   */
  NFC_REQUEST_MAKE_NDEF_READ_ONLY = 5,

  /**
   * NFC_REQUEST_FORMAT
   *
   * Format a tag as NDEF
   *
   * data is NfcSessionId, which is correlates to a technology that was
   * previously discovered with NFC_NOTIFICATION_TECH_DISCOVERED.
   *
   * response is NULL.
   */
  NFC_REQUEST_FORMAT = 6,

  /**
   * NFC_RRQUEST_TRANSCEIVE
   *
   * Send raw data to the tag;
   *
   * response is tag response data.
   */
  NFC_REQUEST_TRANSCEIVE = 7,
} NfcRequestType;

typedef enum {
  NFC_RESPONSE_GENERAL = 1000,

  NFC_RESPONSE_CHANGE_RF_STATE = 1001,

  NFC_RESPONSE_READ_NDEF = 1002,

  NFC_RESPONSE_TAG_TRANSCEIVE = 1003
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
