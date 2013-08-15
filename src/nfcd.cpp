#include "nfcd.h"
#include "NativeNfcManager.h"

#include "NfcService.h"
#include "DeviceHost.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

int main() {

  // 1. Create Native NFC Manager and do initialize
  DeviceHost* pDeviceHost = new DeviceHost(); 
  NativeNfcManager* pNativeNfcManager = new NativeNfcManager(pDeviceHost);
  pNativeNfcManager->initialize();

  // 2. Enable Discovery
  pNativeNfcManager->enableDiscovery();

  // 3. Create thread
  init_nfc_service();

  // Dimi : Copy from netd
  // Eventually we'll become the monitoring thread    
  while(1) {
    sleep(1000);
  }
 
  ALOGI("nfcd exiting");
  //exit(0);
}
