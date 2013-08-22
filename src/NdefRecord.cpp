#include "NdefRecord.h"

void NdefRecord::parse(std::vector<uint8_t>& buf, bool ignoreMbMe)
{
  /*
  std::vector<uint8_t> type;
  std::vector<uint8_t> id;
  std::vector<uint8_t> payload;
  bool isChunk = false;
  uint16_t chunkTnf = -1;
  bool me = false;
  uint32_t index = 0;

  while(!me) {
    uint8_t flag = buf[index++];

    bool mb = (flag & NdefRecord.FLAG_MB) != 0;
    me = (flag & NdefRecord.FLAG_ME) != 0;
    bool cf = (flag & NdefRecord.FLAG_CF) != 0;
    bool sr = (flag & NdefRecord.FLAG_SR) != 0;
    bool il = (flag & NdefRecord.FLAG_IL) != 0;
    uint16_t tnf = (uint16_t)(flag & 0x07);

    if (!mb && records.size() == 0 && !inChunk && !ignoreMbMe) {
        return;
    } else if (mb && records.size() != 0 && !ignoreMbMe) {
        return;
    } else if (inChunk && il) {
        return;
    } else if (cf && me) {
        return;
    } else if (inChunk && tnf != NdefRecord.TNF_UNCHANGED) {
        return;
    } else if (!inChunk && tnf == NdefRecord.TNF_UNCHANGED) {
        return;
    }

    uint32_t typeLength = buf[index++] & 0xFF;
    long payloadLength = sr ? (buf[index++] & 0xFF) : (buffer.getInt() & 0xFFFFFFFFL);
    uint32_t idLength = il ? (buf[index++] & 0xFF) : 0;    
  }
  */
}
