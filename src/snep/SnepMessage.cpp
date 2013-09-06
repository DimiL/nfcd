#include "SnepMessage.h"

SnepMessage::SnepMessage()
{

}

SnepMessage::~SnepMessage()
{

}

SnepMessage* SnepMessage::getGetRequest(int acceptableLength, NdefMessage& ndef)
{
  return NULL;
  //return new SnepMessage(VERSION, REQUEST_GET, 4 + ndef.toByteArray().length, acceptableLength, ndef);
}

SnepMessage* SnepMessage::getPutRequest(NdefMessage& ndef)
{
  return NULL;
  //return new SnepMessage(VERSION, REQUEST_PUT, ndef.toByteArray().length, 0, ndef);
}
