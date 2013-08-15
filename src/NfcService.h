
#ifndef __NFC_SERVICE_H__
#define __NFC_SERVICE_H__

#define MAX_NDEF_RECORD_SIZE 1024

void nfc_service_send_MSG_LLCP_LINK_ACTIVATION(void* pDevice);
void nfc_service_send_MSG_LLCP_LINK_DEACTIVATION(void* pDevice);
void nfc_service_send_MSG_NDEF_TAG();
void nfc_service_send_MSG_SE_FIELD_ACTIVATED();
void nfc_service_send_MSG_SE_FIELD_DEACTIVATED();
void nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS();

void init_nfc_service();

#endif

