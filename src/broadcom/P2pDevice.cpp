#include "P2pDevice.h"

#define LOG_TAG "BroadcomNfc"
#include <cutils/log.h>

P2pDevice::P2pDevice()
{
}

P2pDevice::~P2pDevice()
{
}

bool P2pDevice::connect()
{
  return true;
}

bool P2pDevice::disconnect()
{
  return true;
}

void P2pDevice::transceive()
{
}

void P2pDevice::receive()
{ 
}

bool P2pDevice::send()
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
