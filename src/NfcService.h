#ifndef __NFC_SERVICE_H__
#define __NFC_SERVICE_H__

#include "NfcManager.h"

#define MAX_NDEF_RECORD_SIZE 1024

class NfcService{
private:
  static NfcService* sInstance;
  static NfcManager* sNfcManager;

public:
  virtual ~NfcService();
  void initialize(NfcManager* pNfcManager);

  static NfcService* Instance();

  static void nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_NDEF_TAG(void* pTag);
  static void nfc_service_send_MSG_SE_FIELD_ACTIVATED();
  static void nfc_service_send_MSG_SE_FIELD_DEACTIVATED();
  static void nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS();

  static bool handleReadNdef();

private:
  NfcService();
};

#endif

