
#ifndef __NFC_SERVICE_H__
#define __NFC_SERVICE_H__

#define MAX_NDEF_RECORD_SIZE 1024

class NfcService{
private:
  static NfcService* sInstance;

public:
  virtual ~NfcService();
  void initialize();

  static NfcService* Instance();

  static void nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice);
  static void nfc_service_send_MSG_NDEF_TAG(void* pTag);
  static void nfc_service_send_MSG_SE_FIELD_ACTIVATED();
  static void nfc_service_send_MSG_SE_FIELD_DEACTIVATED();
  static void nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS();

private:
  NfcService();
};

#endif

