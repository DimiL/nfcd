#include "P2pDevice.h"

#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

P2pDevice::P2pDevice()
{
}

P2pDevice::~P2pDevice()
{
}

bool P2pDevice::doConnect()
{
  return true;
}

bool P2pDevice::doDisconnect()
{
  return true;
}

void P2pDevice::doTransceive()
{
}

void P2pDevice::doReceive()
{ 
}

bool P2pDevice::doSend()
{
  return true;
}

int& P2pDevice::getHandle()
{
  return mHandle;
}

int& P2pDevice::getMode()
{
  return mMode;
}
