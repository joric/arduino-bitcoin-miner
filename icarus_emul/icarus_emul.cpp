#include "icarus_emul.h"

const int golden_ob = 0x4679ba4e;
const int golden_nonce = 0x000187a2;
const int workdiv_sig = 0x2e4c8f91;
const int workdiv1 = 0x04C0FDB4;
const int workdiv2 = 0x82540E46;
const int random_nonce = 0xCAFEBABE;

void printHash(uint8_t* hash) {
	char htab[] = "0123456789abcdef";
	int i;
	for (i=0; i<32; i++) {
		printf( "%c", htab[hash[i]>>4] );
		printf( "%c", htab[hash[i]&0xf] );
	}
	printf("\n");
}

int reply(uint32_t sig) {
	print("send: 0x%08x\n", sig);
	int out = htonl(sig);
	Serial.write((char*)&out, 4);
}

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

void strreverse(uint8_t * buf, int len) {
	uint8_t * b = buf;
	uint8_t * e = buf+len-1;
	while (b<e) {
		uint8_t tmp = *b;
		*b++ = *e;
		*e-- = tmp;
	}
}

unsigned int bytereverse(unsigned int x) {
	return ( ((x) << 24) | (((x) << 8) & 0x00ff0000) | (((x) >> 8) & 0x0000ff00) | ((x) >> 24) );
}

char *bufreverse(uint8_t * buf, int len) {
	unsigned int * ibuf = (unsigned int*)buf;
	for (int i=0; i<len/4; i++) {
		ibuf[i] = bytereverse(ibuf[i]);
	}
}

int double_hashing(uint8_t * block_header, int nonce) {
	SHA256_CTX ctx;
	uint8_t hash[32];
	char hex[128];

	*(int*)(block_header+76) = ntohl(nonce);

	sha256_init(&ctx);
	sha256_update(&ctx, block_header, 80);
	sha256_final(&ctx, hash);

	sha256_init(&ctx);
	sha256_update(&ctx, hash, 32);
	sha256_final(&ctx, hash);

	if (*(int*)(hash+32-4)!=0) return -1;

	print("%s (double hashing) nonce: 0x%08x\n", btoh(hex, hash, 32), nonce);

	return 0;
}

inline int midstate_hashing_prepared(uint8_t * buf, uint32_t nonce) {

	uint8_t * midstate = buf;
	uint8_t * block_tail = buf+32;

	SHA256_CTX ctx;
	uint8_t hash[32];
	char hex[128];

	sha256_init(&ctx);
	memcpy(&ctx.state, midstate, 32);

	ctx.datalen = 0;
	ctx.bitlen = 512;

	/*
	print("datalen: %d bitlen: %d\n", ctx.datalen, ctx.bitlen);
	print("%s (midstate)\n", btoh(hex, (uint8_t*)ctx.state, 32));
	print("%s (block_tail)\n", btoh(hex, block_tail, 16));
	*/

	*(uint32_t*)(block_tail+12) = ntohl(nonce);
	sha256_update(&ctx, block_tail, 16);
	sha256_final(&ctx, hash);

	sha256_init(&ctx);
	sha256_update(&ctx, hash, 32);
	sha256_final(&ctx, hash);

	if (*(int*)(hash+32-4)!=0) return -1; // 32-bit
	//if (hash[31]!=0 || hash[30]!=0 || hash[29]!=0) return -1; //24 bit

	print("%s (midstate hashing) nonce: 0x%08x\n", btoh(hex, hash, 32), nonce);

	return 0;
}

int midstate_hashing_prepare(uint8_t * buf, uint8_t * payload) {
	uint8_t * midstate = buf;
	uint8_t * tail = buf+32;

	memcpy(buf, payload, 32);
	strreverse(midstate, 32);

	memcpy(tail, payload+64-12, 12);
	strreverse(tail, 12);
	bufreverse(tail, 12);
}

int midstate_hashing(uint8_t * payload, uint32_t nonce) {
	uint8_t buf[32+16];
	midstate_hashing_prepare(buf, payload);
	return midstate_hashing_prepared(buf, nonce);
}

int main() {
	const int len = 128;
	const int lenb = 80;
	uint8_t data[len];

#if(0)
	// genesis block
	char block[] = "0100000000000000000000000000000000000000000000000000000000000000000000003BA3EDFD7A7B12B27AC72C3E67768F617FC81BC3888A51323A9FB8AA4B1E5E4A29AB5F49FFFF001D1DAC2B7C0101000000010000000000000000000000000000000000000000000000000000000000000000FFFFFFFF4D04FFFF001D0104455468652054696D65732030332F4A616E2F32303039204368616E63656C6C6F72206F6E206272696E6B206F66207365636F6E64206261696C6F757420666F722062616E6B73FFFFFFFF0100F2052A01000000434104678AFDB0FE5548271967F1A67130B7105CD6A828E03909A67962E0EA1F61DEB649F6BC3F4CEF38C4F35504E51EC112DE5C384DF7BA0B8D578A4C702B6BF11D5FAC00000000";
	int nonce = 0x7c2bac1d;
	htob(data, block, 128);
	double_hashing(data, nonce);
#endif

#if(0)
	// test block (reversed)
	char block[] = "0000000120c8222d0497a7ab44a1a2c7bf39de941c9970b1dc7cdc400000079700000000e88aabe1f353238c668d8a4df9318e614c10c474f8cdf8bc5f6397b946c33d7c4e7242c31a098ea500000000000000800000000000000000000000000000000000000000000000000000000000000000000000000000000080020000";
	char midstate[] = "33c5bf5751ec7f7e056443b5aee3800331432c83f404d9de38b94ecbf907b92d";
	char payload[] = "2db907f9cb4eb938ded904f4832c43310380e3aeb54364057e7fec5157bfc5330000000000000000000000008000000000000000a58e091ac342724e7c3dc346";
	int nonce = 0x015e3c06;

	htob(data, block, len);
	bufreverse(data, 80);
	double_hashing(data, nonce);
#endif

#if(0)
	// test payload
	char payload[] = "ce92099c5a80bb81c52990d5c0924c625fd25a535640607d5a4bdf8174e2c8d500000000000000000000000080000000000000000b290c1a42313b4f21b5bcb8";
	int nonce = 0xc5310b8e;
	htob(data, payload, 64);
	midstate_hashing(data, nonce);
#endif

#if(1)
	// golden payload
	char payload2[] = "4679ba4ec99876bf4bfe086082b400254df6c356451471139a3afa71e48f544a000000000000000000000000000000000000000087320b1a1426674f2fa722ce";
	int nonce = 0x000187a2;
	//int nonce = 0xa2870100;
	htob(data, payload2, 64);
	midstate_hashing(data, nonce);
#endif

	const int size = 64;
	char buf[size] = {0};

	char *payload = buf;

	char hex[256];

	print("Opening serial device %s... ", SERIAL_DEVICE);

	Serial.begin(115200);

	print("Reading data...\n");

	while (Serial.available()) {
		int bytes = Serial.readBytes(buf, size);

		if (bytes==64) {

			print("recv: %s (%d bytes)\n", btoh(hex, (uint8_t*)buf, bytes), bytes);

			//print("\nread bytes: %d\n", bytes);

			int sig = ntohl(*(int*)buf);

			//print("recv: 0x%08x\n", sig);

			switch(sig) {
				case golden_ob: reply(golden_nonce); break;
				case workdiv_sig: reply(workdiv1); break;
				default:

					int t0 = millis();
					int seconds = 0;

					uint8_t mbuf[64];
					midstate_hashing_prepare(mbuf, (uint8_t*)buf);

					for (long long int i=0; i<=0xfffffff; i++) {
						uint32_t nonce = (uint32_t)i;

						int t1 = millis();

						if (t1-t0>=1000) {
							seconds++;
							t0 = t1;
							print("Mh/s: %f, nonce: 0x%08x of 0x%08x (%f%%)\r", (float)i/seconds/1000/1000,
								(int)i, 0xffffffff, (float)i/0xffffffff);
						}

						if (!midstate_hashing_prepared(mbuf, nonce)) {
							reply(nonce);
							break;
						}
					}

				break;
			}
		}
	}
}

