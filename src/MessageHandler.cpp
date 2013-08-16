#include "MessageHandler.h"
#include "NfcIpcSocket.h"
#include "NfcUtil.h"
#include "NativeNfcTag.h"

#include <jansson.h>

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include <utils/Log.h>

/*
void MessageHandler::nfcd_messaging_notifyNdefDiscovered(jobject msg)
{
  CLASS_NdefMessage* ndefMsg = (CLASS_NdefMessage*) msg.getReferent();
  jint recordLength = dummyEnv->GetArrayLength(ndefMsg->mRecords);
  LOGD("Creating JSON for %d records", recordLength);

  json_t * records[recordLength];
  int total = 0;
  for(int i=0; i<recordLength; i++) {
    //Get payload length
    CLASS_NdefRecord* record = (CLASS_NdefRecord*) dummyEnv->GetObjectArrayElement(ndefMsg->mRecords, 0).getReferent();

    jint payloadLength = dummyEnv->GetArrayLength((jbyteArray) record->mPayload);
    char* payload = encodeBase64((char*) dummyEnv->GetByteArrayElements((jbyteArray) record->mPayload, JNI_FALSE), payloadLength);
    LOGD("Payload: %s", payload);

    jint typeLength = dummyEnv->GetArrayLength((jbyteArray) record->mType);
    char* type = encodeBase64((char*) dummyEnv->GetByteArrayElements((jbyteArray) record->mType, JNI_FALSE), typeLength);
    LOGD("Type: %s", type);

    jint idLength = dummyEnv->GetArrayLength((jbyteArray) record->mId);
    char* id = encodeBase64((char*) dummyEnv->GetByteArrayElements((jbyteArray) record->mId, JNI_FALSE), idLength);
    LOGD("Id: %s", id);

    records[i] = json_object();
    LOGD("Id: %s", id);

    //TODO: verify this is safe or introduce smarter conversion
    char* tnf = (char*) NFCD_MALLOC(2);
    tnf[0] = '0' + record->mTnf;
    tnf[1] = '\0';
    LOGD("Id: %s", id);

    json_object_set_new(records[i], "tnf",  json_string(tnf));
    json_object_set_new(records[i], "type", json_string(type));
    json_object_set_new(records[i], "id", json_string(id));
    json_object_set_new(records[i], "payload", json_string(payload));
  }

  LOGD("Constructing complete JSON message");
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
  LOGD("Writing JSON message to socket \"%.*s\"", len, rendered);
  json_decref(root);
  if (rendered) {
    writeToSocket(rendered, len);
  }
}
*/

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

