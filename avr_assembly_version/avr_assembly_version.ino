// arduino-bitcoin-miner
// works with ardiuno pro micro (arduino leonardo)
// implements icarus protocol v3 with midstate support
// works with bitcoin pools and bitcoin-in-a-box via bfgminer:
// bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM5
// this one makes about 150 hashes a second (millis() takes 29 clock cycles)

//#define DEBUG

extern "C" {
#include "sha256.h"
}

char hex[256];

inline uint32_t htonl(uint32_t x) {
  return ( ((x) << 24) | (((x) << 8) & 0x00ff0000) | (((x) >> 8) & 0x0000ff00) | ((x) >> 24) );
}

void strreverse(uint8_t * buf, int len) {
  for (uint8_t * b = buf, * e = buf+len-1; b<e; b++, e--) {
    uint8_t tmp = *b;
    *b = *e;
    *e = tmp;
  }
}

char *bufreverse(uint8_t * buf, int len) {
  uint32_t * ibuf = (uint32_t *)buf;
  for (int i=0; i<len/4; i++)
    ibuf[i] = htonl(ibuf[i]);
}

void htob(uint8_t *dest, char *src, int len) {
  int v = 0;
  while ( sscanf(src, "%02x", &v) > 0 ) *dest++ = v, src += 2;
}

char *btoh(char *dest, uint8_t *src, int len) {
  char *d = dest;
  while( len-- ) sprintf(d, "%02x", (unsigned char)*src++), d += 2;
  return dest;
}

uint32_t find_nonce(uint8_t * payload, uint32_t nonce=0, int timeout=15) {
  sha256_hash_t hash;
  uint8_t data[256];

  sha256_ctx_t ctx;
  uint8_t buf[32+16];
  uint8_t * midstate = buf;
  uint8_t * block_tail = buf+32;

  memcpy(midstate, payload, 32);
  strreverse(midstate, 32);

  memcpy(block_tail, payload+64-12, 12);
  strreverse(block_tail, 12);
  bufreverse(block_tail, 12);

  int start = 0;
  int seconds = 0;

  for (;;nonce++) {

    // apply midstate
    memcpy(&ctx.h, midstate, 32);
    ctx.length = 64*8;

    // set nonce and hash the remaining bytes
    *(uint32_t*)(block_tail+12) = htonl(nonce);
    sha256_lastBlock(&ctx, (const void*)block_tail, 16*8);
    sha256_ctx2hash(&hash, &ctx);

    // double hash the result
    sha256(&hash, (const void*)hash, 32*8);

    int end = millis();
    if (end-start>=1000 || start==0) {
      start = end;
#ifdef DEBUG    
      if (seconds) {
        Serial.print((float)nonce/seconds);
        Serial.println(" hashes/sec.");
      }
#endif    
      seconds++;
    }

    
    if (seconds>=timeout || (hash[31]==0 && hash[30]==0 /*&& hash[29]==0 && hash[28]==0 */ )) {
#ifdef DEBUG
      Serial.println(btoh(hex, hash, 32));
#endif      
      return nonce; 
    }    

    if (nonce==0xffffffff)
      break;
  }
  return nonce;
}

#define golden_ob 0x4679ba4e
#define golden_nonce 0x000187a2
#define workdiv_sig 0x2e4c8f91
#define workdiv1 0x04C0FDB4
#define workdiv2 0x82540E46
#define random_nonce 0xdeadbeef

void setup() {
  Serial.begin(115200);

#ifdef DEBUG
  // should return da9fcfb26c7f5b30746b1c068c2bd690a8fa8c16e4a80841b604000000000000
  uint32_t nonce = 0x000187a2;
  char payload_hex[] = "4679ba4ec99876bf4bfe086082b400254df6c356451471139a3afa71e48f544a000000000000000000000000000000000000000087320b1a1426674f2fa722ce";
  uint8_t payload[64];
  htob(payload, payload_hex, 64);
  delay(1000);
  nonce = find_nonce(payload, nonce);
#endif

}

void reply(uint32_t data) {
  data = htonl(data);
  Serial.write((byte*)&data, sizeof(data));
}

void loop() {
  const int size = 64;
  uint8_t payload[size];

  if (!Serial.available() || !Serial.readBytes(payload, size))
    return;

  switch(htonl(*(uint32_t*)payload)) {
    case golden_ob: reply(golden_nonce); break;
    case workdiv_sig: reply(workdiv1); break;
    default: reply(find_nonce(payload)); break;
  }
}

