#include "SnepMessenger.h"

SnepMessenger::SnepMessenger(bool isClient, ILlcpSocket* socket, int fragmentLength) :
mIsClient(isClient),
mSocket(socket),
mFragmentLength(fragmentLength)
{
}

SnepMessenger::~SnepMessenger()
{
}

void SnepMessenger::sendMessage(SnepMessage& msg)
{
}
  
SnepMessage* SnepMessenger::getMessage()
{
  return NULL;
}

void SnepMessenger::close()
{
  if (mSocket != NULL)
    mSocket->close();
}
