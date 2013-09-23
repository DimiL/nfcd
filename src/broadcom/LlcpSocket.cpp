#include "LlcpSocket.h"
#include "PeerToPeer.h"

#define LOG_TAG "nfcd"
#include <cutils/log.h>

LlcpSocket::LlcpSocket(unsigned int handle, int sap, int miu, int rw):
mHandle(handle),
mSap(sap),
mLocalMiu(miu),
mLocalRw(rw)
{}

LlcpSocket::LlcpSocket(unsigned int handle, int miu, int rw):
mHandle(handle),
mLocalMiu(miu),
mLocalRw(rw)
{}

LlcpSocket::~LlcpSocket()
{}

bool LlcpSocket::connectToSap(int sap)
{
  return LlcpSocket::LlcpSocket_doConnect (sap);
}

bool LlcpSocket::connectToService(const char* serviceName)
{
  return LlcpSocket::LlcpSocket_doConnectBy(serviceName);
}

void LlcpSocket::close()
{
  LlcpSocket::LlcpSocket_doClose();
  return;
}

bool LlcpSocket::send(std::vector<uint8_t>& data)
{
  return LlcpSocket::LlcpSocket_doSend(data);
}

int LlcpSocket::receive(std::vector<uint8_t>& recvBuff)
{
  return LlcpSocket::LlcpSocket_doReceive(recvBuff);;
}

int LlcpSocket::getRemoteMiu()
{
  return LlcpSocket::LlcpSocket_doGetRemoteSocketMIU();
}

int LlcpSocket::getRemoteRw()
{
  return LlcpSocket::LlcpSocket_doGetRemoteSocketRW();
}

bool LlcpSocket::LlcpSocket_doConnect (int nSap)
{
  ALOGD ("%s: enter; sap=%d", __FUNCTION__, nSap);

  bool stat = PeerToPeer::getInstance().connectConnOriented (mHandle, nSap);

  ALOGD ("%s: exit", __FUNCTION__);
  return stat ? true : false;
}

bool LlcpSocket::LlcpSocket_doConnectBy (const char* sn)
{
  ALOGD ("%s: enter", __FUNCTION__);

  if (sn == NULL) {
    return false;
  }
  bool stat = PeerToPeer::getInstance().connectConnOriented(mHandle, sn);

  ALOGD ("%s: exit", __FUNCTION__);
  return stat;
}

bool LlcpSocket::LlcpSocket_doSend (std::vector<uint8_t>& data)
{
  UINT8* raw_ptr = new UINT8[data.size()];

  for(uint32_t i = 0; i < data.size(); i++)
    raw_ptr[i] = (UINT8)data[i];

  bool stat = PeerToPeer::getInstance().send(mHandle, raw_ptr, data.size());

  delete[] raw_ptr;

  return stat;
}

int LlcpSocket::LlcpSocket_doReceive(std::vector<uint8_t>& recvBuff)
{
  uint16_t actualLen = 0;
  uint16_t MAX_BUF_SIZE = 4096;

  UINT8* raw_ptr = new UINT8[MAX_BUF_SIZE];
  bool stat = PeerToPeer::getInstance().receive(mHandle, raw_ptr, MAX_BUF_SIZE, actualLen);

  int retval = 0;
  if (stat && (actualLen>0)) {
    for (uint16_t i = 0; i < actualLen; i++)
      recvBuff.push_back(raw_ptr[i]);
    retval = actualLen;
  } else {
    retval = -1;
  }

  delete[] raw_ptr;
  
  return retval;
}

bool LlcpSocket::LlcpSocket_doClose()
{   
  ALOGD ("%s: enter", __FUNCTION__);
    
  bool stat = PeerToPeer::getInstance().disconnectConnOriented (mHandle);

  ALOGD ("%s: exit", __FUNCTION__);
  return true;  // TODO: stat?
}

int LlcpSocket::LlcpSocket_doGetRemoteSocketMIU ()
{
  ALOGD ("%s: enter", __FUNCTION__);

  int miu = PeerToPeer::getInstance().getRemoteMaxInfoUnit(mHandle);

  ALOGD ("%s: exit", __FUNCTION__);
  return miu;
}

int LlcpSocket::LlcpSocket_doGetRemoteSocketRW ()
{
  ALOGD ("%s: enter", __FUNCTION__);

  int rw = PeerToPeer::getInstance().getRemoteRecvWindow (mHandle);

  ALOGD ("%s: exit", __FUNCTION__);
  return rw;
}
