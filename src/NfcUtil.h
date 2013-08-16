
#ifndef __NFC_UTIL_H__
#define __NFC_UTIL_H__

#include <stdio.h>

class NfcUtil{

public:
  static char* encodeBase64(const char *input, size_t length);
  static char* decodeBase64(unsigned char *input, size_t length, size_t *out_length);
  static char* getTechString(int techIndex);

private:
  NfcUtil();
};

#endif

