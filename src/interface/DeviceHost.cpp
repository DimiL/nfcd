#include "DeviceHost.h"
#include "NfcService.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

void DeviceHost::notifyTagDiscovered(INfcTag* pTag)
{
  NfcService::notifyTagDiscovered(pTag);
}

void DeviceHost::notifyTargetDeselected()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifyTransactionListeners()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifyLlcpLinkActivated(IP2pDevice* pDevice)
{
  NfcService::notifyLlcpLinkActivated(pDevice);
}

void DeviceHost::notifyLlcpLinkDeactivated(IP2pDevice* pDevice)
{
  NfcService::notifyLlcpLinkDeactivated(pDevice);
}

void DeviceHost::notifyLlcpLinkFirstPacketReceived()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifySeFieldActivated()
{
  ALOGE("%s: not implement", __FUNCTION__);
}

void DeviceHost::notifySeFieldDeactivated()
{
  ALOGE("%s: not implement", __FUNCTION__);
}
