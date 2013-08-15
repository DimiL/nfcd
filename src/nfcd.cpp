#include "nfcd.h"
#include "NativeNfcManager.h"

#include "NfcService.h"
#include "NfcIpcSocket.h"
#include "DeviceHost.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

int main() {

  // 1. Create Native NFC Manager and do initialize
  NativeNfcManager* pNativeNfcManager = new NativeNfcManager();
  pNativeNfcManager->initialize();

  // 2. Enable Discovery
  pNativeNfcManager->enableDiscovery();

  // 3. Create thread
  init_nfc_service();

  // 4. Create IPC socket & enter while loop
  NfcIpcSocket* pNfcIpcSocket = new NfcIpcSocket();
  pNfcIpcSocket->initialize();
  pNfcIpcSocket->loop();

  //exit(0);
}
