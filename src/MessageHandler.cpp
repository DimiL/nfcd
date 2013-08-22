#include "MessageHandler.h"
#include "NfcService.h"
#include "NfcIpcSocket.h"
#include "NfcUtil.h"
#include "NativeNfcTag.h"
#include "NdefMessage.h"
#include "NdefRecord.h"

#include <jansson.h>
#include <map>
#include <string>

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include <utils/Log.h>

static std::map<std::string, int> gMessageTypeMap;

void MessageHandler::initialize()
{
  // Initialize with private strings...
  gMessageTypeMap["ndefDiscovered"] = NOTIFY_NDEF_DISCOVERED;
  gMessageTypeMap["techDiscovered"] = NOTIFY_TECH_DISCOVERED;
  gMessageTypeMap["transceiveReq"] = NOTIFY_TRANSCEIVE_REQ;
  gMessageTypeMap["transceiveRsp"] = NOTIFY_TRANSCEIVE_RSP;
  gMessageTypeMap["ndefWriteRequest"] = NOTIFY_NDEF_WRITE_REQUEST;
  gMessageTypeMap["ndefDetailsRequest"] = NOTIFY_NDEF_DETAILS_REQUEST;
  gMessageTypeMap["readNdef"] = NOTIFY_READ_NDEF;
  gMessageTypeMap["ndefDisconnected"] = NOTIFY_NDEF_DISCONNECTED;
  gMessageTypeMap["requestStatus"] = NOTIFY_REQUEST_STATUS;
  gMessageTypeMap["ndefPushRequest"] = NOTIFY_NDEF_PUSH_REQUEST;
  gMessageTypeMap["secureElementActivated"] = NOTIFY_SECURE_ELEMENT_ACTIVATE;
  gMessageTypeMap["secureElementDeactivated"] = NOTIFY_SECURE_ELEMENT_DEACTIVATE;
  gMessageTypeMap["secureElementTransaction"] = NOTIFY_SECURE_ELEMENT_TRANSACTION;
}

void MessageHandler::messageNotifyNdefDiscovered(NdefMessage* ndefMsg)
{
  int recordLength = ndefMsg->mRecords.size();
  ALOGD("Creating JSON for %d records", recordLength);

  json_t * records[recordLength];
  int total = 0;
  char* buf = NULL;

  // Mozilla : TODO : check if there is memory leak here
  for(int i = 0; i < recordLength; i++) {
    //Get payload length
    NdefRecord* record = &ndefMsg->mRecords[i];
    
    int payloadLength = record->mPayload.size();
    buf = new char(payloadLength);
    for(int idx = 0; idx < payloadLength; idx++)  buf[idx] = record->mPayload[idx];
    char* payload = NfcUtil::encodeBase64(buf, payloadLength);
    ALOGD("Payload: %s", payload);
    
    int typeLength = record->mType.size();
    buf = new char(typeLength);
    for(int idx = 0; idx < typeLength; idx++)  buf[idx] = record->mType[idx];
    char* type = NfcUtil::encodeBase64(buf, typeLength);
    ALOGD("Type: %s", type);

    int idLength = record->mId.size();
    buf = new char(idLength);
    for(int idx = 0; idx < idLength; idx++)  buf[idx] = record->mId[idx];
    char* id = NfcUtil::encodeBase64(buf, idLength);
    ALOGD("Id: %s", id);

    records[i] = json_object();

    //TODO: verify this is safe or introduce smarter conversion
    char* tnf = (char*) malloc(2);
    tnf[0] = '0' + record->mTnf;
    tnf[1] = '\0';

    json_object_set_new(records[i], "tnf",  json_string(tnf));
    json_object_set_new(records[i], "type", json_string(type));
    json_object_set_new(records[i], "id", json_string(id));
    json_object_set_new(records[i], "payload", json_string(payload));
  }

  ALOGD("Constructing complete JSON message");
  json_t* root = json_object();
  json_object_set_new(root, "type", json_string("ndefDiscovered"));

  json_t* jsonArray = json_array();
  for(int i=0; i<recordLength; i++) {
    json_array_append(jsonArray, records[i]);
  }

  json_t* content = json_object();
  json_object_set_new(content, "records", jsonArray);
  json_object_set_new(root, "content", content);

  char *rendered = json_dumps(root, JSON_PRESERVE_ORDER);
  size_t len = strlen(rendered);
  ALOGD("Writing JSON message to socket \"%.*s\"", len, rendered);
  json_decref(root);
  if (rendered) {
    NfcIpcSocket::writeToOutgoingQueue(rendered, strlen(rendered));
  }
}

void MessageHandler::messageNotifyNdefDetails(int maxNdefMsgLength, int state)
{
  //Create JSON record
  json_t *root,*content;
  root = json_object();
  char *message = NULL;
  ALOGD("In nfcd_messaging_notifyNdefDetails...");
  json_object_set_new(root, "type", json_string("ndefDetailsResponse"));
  json_object_set_new(root, "content", content=json_object());
  json_object_set_new(content, "maxndefMsgLen", json_integer(maxNdefMsgLength));
  json_object_set_new(content, "cardstate", json_integer(state));

  char *rendered = json_dumps(root, JSON_PRESERVE_ORDER);
  json_decref(root);
  ALOGD("%s rendered ", rendered);
  if (rendered) {
    NfcIpcSocket::writeToOutgoingQueue(rendered, strlen(rendered));
  }
}

void MessageHandler::messageNotifyNdefDisconnected()
{
  MessageHandler::messageNotifyNdefDisconnected(NULL);
}

void MessageHandler::messageNotifyNdefDisconnected(const char *message)
{
  //Create JSON record
  json_t *root,*content;
  root = json_object();
  json_object_set_new(root, "type", json_string("ndefDisconnected"));
  json_object_set_new(root, "content", content=json_object());
  if (message != NULL) {
    json_object_set_new(content, "message", json_string(message));
  }
  char *rendered = json_dumps(root, JSON_PRESERVE_ORDER);

  json_decref(root);

  if (rendered) {
    NfcIpcSocket::writeToOutgoingQueue(rendered, strlen(rendered));
  }
}

void MessageHandler::messageNotifyTechDiscovered(NativeNfcTag* pNativeNfcTag)
{  
  json_t *root,*content;
  json_t* jsonTechArray = json_array();
  int i = 0;

  int techIndex = 0;
  int techArrSize = pNativeNfcTag->mTechList.size();
  json_t* records[techArrSize];

  for(techIndex = 0; techIndex < techArrSize; techIndex++) {
    for (int i = 0; i < techIndex; i++) {
      if (pNativeNfcTag->mTechHandles[i] == pNativeNfcTag->mTechHandles[techIndex]) {
        ALOGD("Here... handles[i]  %d", pNativeNfcTag->mTechHandles[i]);
        continue;  // don't check duplicate handles
      }
    }
    ALOGD("Tech Val %s", NfcUtil::getTechString(pNativeNfcTag->mTechList[techIndex]));
    json_array_append(jsonTechArray, json_string(NfcUtil::getTechString(pNativeNfcTag->mTechList[techIndex])));
  }

  root = json_object();
  json_object_set_new(root, "type", json_string("techDiscovered"));
  json_object_set_new(root, "content", content=json_object());
  if (pNativeNfcTag->mTechList.size() != 0) {
    json_object_set_new(content, "tech", jsonTechArray);
  }

  char *rendered = json_dumps(root, JSON_PRESERVE_ORDER);

  json_decref(root);

  if (rendered) {
    NfcIpcSocket::writeToOutgoingQueue(rendered, strlen(rendered));
  }
}

void MessageHandler::messageNotifyRequestStatus(const char *requestId, int status, char *message)
{
  //Create JSON record
  json_t *root,*content;
  root = json_object();
  json_object_set_new(root, "type", json_string("requestStatus"));
  json_object_set_new(root, "content", content=json_object());
  json_object_set_new(content, "requestId", json_string(NfcUtil::encodeBase64(requestId, strlen(requestId))));
  if (status == true) {
    json_object_set_new(content, "status", json_string("OK"));
  } else {
    json_object_set_new(content, "status", json_string("FAILURE"));
  }
  if (message != NULL) {
    json_object_set_new(content, "message", json_string(message));
  }
  char *rendered = json_dumps(root, JSON_PRESERVE_ORDER);

  json_decref(root);
  // Write rendered message to nfcd gecko (pass raw pointer ownership):
  if (rendered) {
    NfcIpcSocket::writeToOutgoingQueue(rendered, strlen(rendered));
  }
}

void MessageHandler::messageNotifySecureElementFieldActivated()
{
  //Create JSON record
  json_t *root,*content;
  root = json_object();
  json_object_set_new(root, "type", json_string("secureElementActivated"));
  json_object_set_new(root, "content", content=json_object()); // Empty content body

  char *rendered = json_dumps(root, JSON_PRESERVE_ORDER);
  ALOGD("=== SecureElement Activated: (%s) ===", rendered);

  json_decref(root);
  // Write rendered message to nfcd gecko (pass raw pointer ownership):
  if (rendered) {
    NfcIpcSocket::writeToOutgoingQueue(rendered, strlen(rendered));
  }
}

void MessageHandler::messageNotifySecureElementFieldDeactivated()
{
  //Create JSON record
  json_t *root,*content;
  root = json_object();
  json_object_set_new(root, "type", json_string("secureElementDeactivated"));
  json_object_set_new(root, "content", content=json_object()); // Empty content body

  char *rendered = json_dumps(root, JSON_PRESERVE_ORDER);
  ALOGD("=== SecureElement Deactivated: (%s) ===", rendered);

  json_decref(root);
  // Write rendered message to nfcd gecko (pass raw pointer ownership):
  if (rendered) {
    NfcIpcSocket::writeToOutgoingQueue(rendered, strlen(rendered));
  }
}

void MessageHandler::processRequest(const char *input, size_t length)
{
  unsigned int messageType;

  if (retrieveMessageType(input, length, &messageType)) {
    ALOGI("MessageType: (%d)", messageType);

    switch (messageType) {
      case NOTIFY_NDEF_WRITE_REQUEST:
        handleWriteNdef(input, length);
        break;
      case NOTIFY_NDEF_PUSH_REQUEST:
        handleNdefPush(input, length);
        break;
      case NOTIFY_REQUEST_STATUS:
      case NOTIFY_NDEF_DISCOVERED:
      case NOTIFY_NDEF_DISCONNECTED:
        break;
      case NOTIFY_NDEF_DETAILS_REQUEST:
        handleNdefDetailsRequest();
        break;
      case NOTIFY_READ_NDEF:
        handleReadNdef();
        break;
      case NOTIFY_TRANSCEIVE_REQ:
        handleTransceiveReq(input, length);
        break;
      default:
        break;
    }
  }
}

/**
 * Retrieve Message Type id from JSON data.
 */
bool MessageHandler::retrieveMessageType(const char *input, size_t length, unsigned int *outTypeId)
{
  if (length == 0 && input == NULL)
    return false;

  bool ret = true;
  json_error_t error;
  json_t *root = json_loads(input, 0, &error);
  json_t *item;
  char *msgType = NULL;
  if (!root) {
    ALOGE("retrieveMessageType: unable to parse input: (%.*s)", length, input);
    return false;
  }

  item = json_object_get(root, "type");
  if (item != NULL && json_is_string(item)) {
    msgType = (char*)json_string_value(item);
  }

  if (msgType) {
    *outTypeId = gMessageTypeMap[msgType];
  } else {
    ret = false;
  }
  json_decref(root);

  return ret;
}

bool MessageHandler::handleWriteNdef(const char *input, size_t length)
{
  return true;
}

bool MessageHandler::handleNdefPush(const char *input, size_t length)
{
  return true;
}

bool MessageHandler::handleNdefDetailsRequest()
{
  return true;
}

bool MessageHandler::handleReadNdef()
{
  NfcService::handleReadNdef();
  return true;
}

bool MessageHandler::handleTransceiveReq(const char *input, size_t length)
{
  return true;
}
