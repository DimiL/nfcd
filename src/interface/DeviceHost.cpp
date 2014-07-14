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

void DeviceHost::notifyTransactionEvent(TransactionEvent* pEvent)
{
  NfcService::notifySETransactionEvent(pEvent);
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

TransactionEvent::TransactionEvent()
 : aidLen(0)
 , aid(NULL)
 , payloadLen(0)
 , payload(NULL)
{
}

TransactionEvent::~TransactionEvent()
{
  delete aid;
  delete payload;
}
