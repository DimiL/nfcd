
#ifndef __NFC_MESSAGE_HANDLER_H__
#define __NFC_MESSAGE_HANDLER_H__

class NativeNfcTag;

class MessageHandler{
public:
  static void messageNotifyNdefDetails(int maxNdefMsgLength, int state);
  static void messageNotifyNdefDisconnected(const char *message);
  static void messageNotifyTechDiscovered(NativeNfcTag* pNativeNfcTag);
  static void messageNotifyRequestStatus(const char *requestId, int status, char *message);
  static void messageNotifySecureElementFieldActivated();
  static void messageNotifySecureElementFieldDeactivated();

private:
  MessageHandler();
};

#endif

