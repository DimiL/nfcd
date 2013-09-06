#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "NfcService.h"
#include "NfcManager.h"
#include "SnepClient.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

SnepClient::SnepClient()
{

}

SnepClient::~SnepClient()
{

}

void SnepClient::put(NdefMessage& msg)
{

}

SnepMessage* SnepClient::get(NdefMessage& msg)
{
  return NULL;
}

void SnepClient::connect()
{

}

void close()
{

}
