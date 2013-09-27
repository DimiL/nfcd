#include "DeviceHost.h"
#include "NfcService.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

DeviceHost::DeviceHost()
{
}

DeviceHost::~DeviceHost()
{
}

void DeviceHost::notifyTagDiscovered(void* pTag)
{
  NfcService::notifyTagDiscovered(pTag);
}

void DeviceHost::notifyTargetDeselected()
{
  ALOGE("function %s not implement", __FUNCTION__);
}

void DeviceHost::notifyTransactionListeners()
{
  ALOGE("function %s not implement", __FUNCTION__);
}

void DeviceHost::notifyLlcpLinkActivation(void* pDevice)
{
  NfcService::notifyLlcpLinkActivation(pDevice);
}

void DeviceHost::notifyLlcpLinkDeactivated(void* pDevice)
{
  NfcService::notifyLlcpLinkDeactivation(pDevice);
}

void DeviceHost::notifyLlcpLinkFirstPacketReceived()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifySeFieldActivated()
{ 
  ALOGE("%s: not implement", __FUNCTION__);
  //NfcService::notifySEFieldActivated();
}

void DeviceHost::notifySeFieldDeactivated()
{
  ALOGE("%s: not implement", __FUNCTION__);
  //NfcService::notifySEFieldDeactivated();
}
