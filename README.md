# nfcd

A daemon running on FirefoxOS to access NFC libraries.

Firefox OS will communicate with this daemon by IPC, with messages defined in 
src/NfcGonkMessage.h, to perfrom NFC functionalities.

This daemon will link to native NFC library on the device, with porting layer
is located in src/{nfc-chip-vendor}, currently the supported NFC chip is
Broadcom.
