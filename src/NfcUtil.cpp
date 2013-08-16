#include "NfcUtil.h"

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#define LOG_TAG "nfcd"
#include <cutils/log.h>

char* NfcUtil::encodeBase64(const char *input, size_t length)
{
  if(length > 0) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *)malloc(bptr->length+1);
    memcpy(buff, bptr->data, bptr->length);
    buff[bptr->length] = 0;

    BIO_free_all(b64);

    return buff;
  } else {
    return "";
  }
}

char* NfcUtil::decodeBase64(unsigned char *input, size_t length, size_t *out_length)
{
  BIO *b64, *bmem;
  size_t decodedLen;

  char *buffer = (char*)malloc(length+1);
  if (buffer == NULL) {
    return NULL;
  }
  memset(buffer, 0, length);

  b64 = BIO_new(BIO_f_base64());
  if (b64 == NULL) {
    return NULL;
  }
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  bmem = BIO_new_mem_buf(input, length);
  bmem = BIO_push(b64, bmem);

  decodedLen = BIO_read(bmem, buffer, length);
  buffer[decodedLen] = 0;
  *out_length = decodedLen;

  BIO_free_all(bmem);
  return buffer;
}

char* NfcUtil::getTechString(int techIndex) {
  switch(techIndex) {
    case 1: return "NFC_A";
    case 2: return "NFC_B";
    case 3: return "ISO_DEP";
    case 4: return "NFC_F";
    case 5: return "NFC_V";
    case 6: return "NDEF";
    case 7: return "NDEF_FORMATABLE";
    case 8: return "MIFARE_CLASSIC";
    case 9: return "MIFARE_ULTRALIGHT";
    case 10: return "NFC_BARCODE";
    default : break;
  }
  return NULL;
}
