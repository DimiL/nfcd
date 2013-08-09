#include "nfcd.h"
#include "NativeNfcManager.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

int main() {

    // 1. Create Native NFC Manager and do initialize
    NativeNfcManager* pNativeNfcManager = new NativeNfcManager();
    // 2. Enable Discovery

    // 3. Create thread
}
