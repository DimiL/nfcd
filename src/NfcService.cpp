#include "NfcService.h"

#undef LOG_TAG
#define LOG_TAG "nfcd"
#include <utils/Log.h>

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

static pthread_t thread_id;
static sem_t thread_sem;

typedef enum {
  MSG_UNDEFINED = 0,
  MSG_LLCP_LINK_ACTIVATION,
  MSG_LLCP_LINK_DEACTIVATION,
  MSG_NDEF_TAG,
  MSG_SE_FIELD_ACTIVATED,
  MSG_SE_FIELD_DEACTIVATED,
  MSG_SE_NOTIFY_TRANSACTION_LISTENERS
} MSG_TYPE;

static MSG_TYPE msg_type = MSG_UNDEFINED;

void nfc_service_send_MSG_LLCP_LINK_ACTIVATION()
{
  msg_type = MSG_LLCP_LINK_ACTIVATION;
  sem_post(&thread_sem);
}

void nfc_service_send_MSG_LLCP_LINK_DEACTIVATION()
{
  msg_type = MSG_LLCP_LINK_DEACTIVATION;
  sem_post(&thread_sem);
}

void nfc_service_send_MSG_NDEF_TAG()
{
  msg_type = MSG_NDEF_TAG;
  sem_post(&thread_sem);
}

void nfc_service_send_MSG_SE_FIELD_ACTIVATED()
{
  msg_type = MSG_SE_FIELD_ACTIVATED;
  sem_post(&thread_sem);
}

void nfc_service_send_MSG_SE_FIELD_DEACTIVATED()
{
  msg_type = MSG_SE_FIELD_DEACTIVATED;
  sem_post(&thread_sem);
}

void nfc_service_send_MSG_SE_NOTIFY_TRANSACTION_LISTENERS()
{
  msg_type = MSG_SE_NOTIFY_TRANSACTION_LISTENERS;
  sem_post(&thread_sem);
}

static void NfcService_MSG_LLCP_LINK_ACTIVATION()
{
  ALOGD("LLCP Activation message");
}

static void *service_thread(void *arg)
{
  pthread_setname_np(pthread_self(), "NFCService thread");

  ALOGD("NFCService started");
  while(true) {
    if(sem_wait(&thread_sem)) {
      ALOGE("NFCService: Failed to wait for semaphore");
      abort();
    }

    switch(msg_type) {
      case MSG_LLCP_LINK_ACTIVATION:
        NfcService_MSG_LLCP_LINK_ACTIVATION();
        break;
      case MSG_LLCP_LINK_DEACTIVATION:
        break;
      case MSG_NDEF_TAG:
        break;
      case MSG_SE_FIELD_ACTIVATED:
        break;
      case MSG_SE_FIELD_DEACTIVATED:
        break;
      case MSG_SE_NOTIFY_TRANSACTION_LISTENERS:
        break;
      default:
        ALOGE("NFCService bad message");
        abort();
    }
  }

  return NULL;
}

void init_nfc_service()
{
  if(sem_init(&thread_sem, 0, 0) == -1)
  {
    ALOGE("init_nfc_service Semaphore creation failed");
    abort();
  }

  if(pthread_create(&thread_id, NULL, service_thread, NULL) != 0)
  {
    ALOGE("init_nfc_service pthread_create failed");
    abort();
  }
}
