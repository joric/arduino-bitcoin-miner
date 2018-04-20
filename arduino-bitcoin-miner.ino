// arduino-bitcoin-miner
// works with ardiuno pro micro (arduino leonardo)
// implements icarus protocol v3 with midstate support
// works with bitcoin pools and bitcoin-in-a-box via bfgminer:
// bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM5
// this one makes about 50 hashes/second

//#define DEBUG
#undef LED_BUILTIN
#define LED_BUILTIN 17 // pro micro

char hex[256];
uint8_t data[256];
int start = 0;
int seconds = 0;
uint8_t hash[32];

#define SHA256_BLOCK_SIZE 32

typedef struct {
  uint8_t data[64];
  uint32_t datalen;
  unsigned long long bitlen;
  uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[]);

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const uint32_t k[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]) {
  uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

  for (i = 0, j = 0; i < 16; ++i, j += 4)
    m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) | ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
  for ( ; i < 64; ++i)
    m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

  a = ctx->state[0];
  b = ctx->state[1];
  c = ctx->state[2];
  d = ctx->state[3];
  e = ctx->state[4];
  f = ctx->state[5];
  g = ctx->state[6];
  h = ctx->state[7];

  for (i = 0; i < 64; ++i) {
    t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
    t2 = EP0(a) + MAJ(a,b,c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

void sha256_init(SHA256_CTX *ctx)
{
  ctx->datalen = 0;
  ctx->bitlen = 0;
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
}

void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len) {
  uint32_t i;

  for (i = 0; i < len; ++i) {
    ctx->data[ctx->datalen] = data[i];
    ctx->datalen++;
    if (ctx->datalen == 64) {
      sha256_transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

void sha256_final(SHA256_CTX *ctx, uint8_t hash[]) {
  uint32_t i;

  i = ctx->datalen;

  // Pad whatever data is left in the buffer.
  if (ctx->datalen < 56) {
    ctx->data[i++] = 0x80;
    while (i < 56)
      ctx->data[i++] = 0x00;
  }
  else {
    ctx->data[i++] = 0x80;
    while (i < 64)
      ctx->data[i++] = 0x00;
    sha256_transform(ctx, ctx->data);
    memset(ctx->data, 0, 56);
  }

  // Append to the padding the total message's length in bits and transform.
  ctx->bitlen += ctx->datalen * 8;
  ctx->data[63] = ctx->bitlen;
  ctx->data[62] = ctx->bitlen >> 8;
  ctx->data[61] = ctx->bitlen >> 16;
  ctx->data[60] = ctx->bitlen >> 24;
  ctx->data[59] = ctx->bitlen >> 32;
  ctx->data[58] = ctx->bitlen >> 40;
  ctx->data[57] = ctx->bitlen >> 48;
  ctx->data[56] = ctx->bitlen >> 56;
  sha256_transform(ctx, ctx->data);

  // Since this implementation uses little endian byte ordering and SHA uses big endian,
  // reverse all the bytes when copying the final state to the output hash.
  for (i = 0; i < 4; ++i) {
    hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
  }
}

#define ntohl(x) ( (((uint32_t)x)<<24 & 0xFF000000UL) | (((uint32_t)x)<< 8 & 0x00FF0000UL) | (((uint32_t)x)>> 8 & 0x0000FF00UL) | (((uint32_t)x)>>24 & 0x000000FFUL) )

inline uint32_t bytereverse(uint32_t x) {
  return ( ((x) << 24) | (((x) << 8) & 0x00ff0000) | (((x) >> 8) & 0x0000ff00) | ((x) >> 24) );  
}

const uint32_t golden_ob = 0x4679ba4e;
const uint32_t golden_nonce = 0x000187a2;
const uint32_t workdiv_sig = 0x2e4c8f91;
const uint32_t workdiv1 = 0x04C0FDB4;
const uint32_t workdiv2 = 0x82540E46;
const uint32_t random_nonce = 0xdeadbeef;

void strreverse(uint8_t * buf, int len) {
  uint8_t * b = buf;
  uint8_t * e = buf+len-1;
  while (b<e) {
    uint8_t tmp = *b;
    *b++ = *e;
    *e-- = tmp;
  }
}

char *bufreverse(uint8_t * buf, int len) {
  uint32_t * ibuf = (uint32_t *)buf;
  for (int i=0; i<len/4; i++) {
    ibuf[i] = bytereverse(ibuf[i]);
  }
}

int midstate_hashing(uint8_t *hash, uint8_t * payload, uint32_t nonce) {
  SHA256_CTX ctx;
  uint8_t buf[32+16];
  uint8_t * midstate = buf;
  uint8_t * block_tail = buf+32;

  memcpy(midstate, payload, 32);
  strreverse(midstate, 32);

  memcpy(block_tail, payload+64-12, 12);
  strreverse(block_tail, 12);
  bufreverse(block_tail, 12);

  // apply midstate
  memcpy(&ctx.state, midstate, 32);
  ctx.datalen = 0;
  ctx.bitlen = 512;

  // set nonce and hash the remaining bytes
  *(uint32_t*)(block_tail+12) = bytereverse(nonce);
  sha256_update(&ctx, block_tail, 16);
  sha256_final(&ctx, hash);

  // double hash the result
  sha256_init(&ctx);
  sha256_update(&ctx, hash, 32);
  sha256_final(&ctx, hash);

  if ( hash[31] || hash[30] ) return -1; // simplified shares (16 zero bits)
  //if (*(uint32_t*)(hash+32-4)!=0) return -1; // real shares (32 zero bits)

  return 0;
}

int reply(uint32_t data) {
  data = ntohl(data);
  Serial.write((byte*)&data, sizeof(data));
}

const int size = 64;
char buf[size] = {0};

void htob(uint8_t *dest, char *src, int len) {
  int v = 0;
  while ( sscanf(src, "%02x", &v) > 0 )
    *dest++ = v, src += 2;
}

char *btoh(char *dest, uint8_t *src, int len) {
  char *d = dest;
  while( len-- ) sprintf(d, "%02x", (unsigned char)*src++), d += 2;
  return dest;
}

void setup() {
  Serial.begin(115200);

  for (int i=0; i<20; i++) {
    digitalWrite(LED_BUILTIN, i%2 ? HIGH : LOW);
    delay(50);
  }

#ifdef DEBUG
  // magic payload, hash da9fcfb26c7f5b30746b1c068c2bd690a8fa8c16e4a80841b604000000000000
  char payload[] = "4679ba4ec99876bf4bfe086082b400254df6c356451471139a3afa71e48f544a000000000000000000000000000000000000000087320b1a1426674f2fa722ce";
  uint32_t nonce = 0x000187a2;
  htob(data, payload, 64);
  midstate_hashing(hash, data, nonce);
  Serial.println(btoh(hex, hash, 32)); 
#endif
}

void loop() {
  if (Serial.available() > 0) {
    int bytes = Serial.readBytes(buf, size);
    if (bytes>0) {
      uint32_t sig = ntohl(*(uint32_t*)buf);
      switch(sig) {
        case golden_ob: reply(golden_nonce); break;
        case workdiv_sig: reply(workdiv1); break;

        default:
          start = -1;
          for (uint32_t nonce=0;;nonce++) {

#ifdef DEBUG
            int end = millis();
            if (end-start>=1000 || start==-1) {
              start = end;
              if (seconds) {
                Serial.print((float)nonce/seconds);
                Serial.println(" hashes/sec.");
              }
              seconds++;
            }
#endif

            if (!midstate_hashing(hash, (uint8_t*)buf, nonce)) {
              reply(nonce);
              break;
            }

            if (nonce==0xffffffff)
              break;
          }
        break;

      }
    }
  }
}

