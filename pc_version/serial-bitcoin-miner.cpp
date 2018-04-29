// win32 serial port bitcoin miner (build with tdm-gcc)
// com0com emulator: https://code.google.com/archive/p/powersdr-iq/downloads
// use com pairs, e.g. you listen on COM8 and specify COM9 for the bfgminer:
// bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM9

int serial_port = 8;
const int threads_num = 4;

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <pthread.h>

#define uint8_t unsigned char
#define uint32_t unsigned int
#define delay(ms) Sleep(ms)
#define millis() GetTickCount()

uint32_t ans[threads_num];
float mhs[threads_num];

inline uint32_t htonl(uint32_t x) {
  return ( ((x) << 24) | (((x) << 8) & 0x00ff0000) | (((x) >> 8) & 0x0000ff00) | ((x) >> 24) );
}

void strreverse(uint8_t * buf, int len) {
	for (uint8_t *b = buf, *e = buf+len-1, tmp; b<e; tmp=*b, *b++=*e, *e--=tmp);
}

uint8_t *bufreverse(uint8_t * buf, int len) {
	for (uint32_t i=0, *ibuf = (uint32_t *)buf; i<len/4; ibuf[i] = htonl(ibuf[i]), i++);
	return buf;
}

uint8_t *htob(uint8_t *dest, const char *src, int len=0, int v=0) {
	for (uint8_t *p=dest; sscanf(src, "%02x", &v)>0 && (len==0 || len>p-dest); *p++ = v, src += 2);
	return dest;
}

char *btoh(char *dest, uint8_t *src, int len) {
	for (char *d = dest; len--; sprintf(d, "%02x", (unsigned char)*src++), d += 2);
	return dest;
}

int serial_write(HANDLE hSerial, unsigned char * buf, int size) {
	OVERLAPPED osWrite = {0};
	DWORD dwWritten;
	DWORD dwRes;
	return WriteFile(hSerial, buf, size, &dwWritten, &osWrite);
}

int serial_read(HANDLE hSerial, unsigned char * buf, int size) {
	DWORD bytes_read = 0;
	ReadFile(hSerial, buf, size, &bytes_read, NULL);
	if (bytes_read) printf("recv: %d bytes\n", (int)bytes_read);
	return (int)bytes_read;
}

int serial_open(HANDLE * pSerial, int speed) {

	HANDLE hSerial;

	DCB dcbSerialParams = {0};
	COMMTIMEOUTS timeouts = {0};
	char device[32];
	sprintf (device, "\\\\.\\COM%d", serial_port);

	printf("Opening COM%d... ", serial_port);

	hSerial = CreateFile(device, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

	if (hSerial == INVALID_HANDLE_VALUE) {
		printf("Error\n");
		return -1;
	}

	printf("OK\n");

	// Set device parameters (115200 baud, arduino default 1 stop bit, no parity)
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	if (GetCommState(hSerial, &dcbSerialParams) == 0) {
		printf("Error getting device state\n");
		CloseHandle(hSerial);
		return -1;
	}

	dcbSerialParams.BaudRate = speed;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;

	if(SetCommState(hSerial, &dcbSerialParams) == 0) {
		printf("Error setting device parameters\n");
		CloseHandle(hSerial);
		return -1;
	}

	// Set COM port timeout settings
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	if(SetCommTimeouts(hSerial, &timeouts) == 0)
	{
		printf("Error setting timeouts\n");
		CloseHandle(hSerial);
		return -1;
	}

	*pSerial = hSerial;

	return 0;
}

struct ArduinoSerial {
	HANDLE hSerial;
	void begin(int speed) { serial_open(&hSerial, speed); }
	int available() { return 1; }
	int readBytes(unsigned char * buf, int size) { return serial_read(hSerial, buf, size); }
	int write(unsigned char * buf, int size) { return serial_write(hSerial, buf, size); }
	void println(const char * s=0) { printf("%s\n", s); };
	void print(float f) { printf("%f", f); };
	void print(char * s) {};
	void print(char c) { serial_write(hSerial, (unsigned char*)&c, 1); };
}; 

ArduinoSerial Serial;

volatile int terminate = 0;

uint32_t find_nonce(uint8_t * payload, uint32_t nonce=0, uint32_t end_nonce=0xffffffff, int thread=0, int timeout=10) {
	char hex[128];

	printf("> 0x%08x, payload: %s...\n", nonce, btoh(hex, payload, 32-3));

	uint8_t buf[32+16];
	uint8_t * midstate = buf;
	uint8_t * block_tail = buf+32;

	memcpy(midstate, payload, 32);
	strreverse(midstate, 32);

	memcpy(block_tail, payload+64-12, 12);
	strreverse(block_tail, 12);
	bufreverse(block_tail, 12);

	uint8_t hash[32];
	SHA256_CTX ctx;
	uint32_t start_nonce = nonce;

	terminate = 0;

	for (int start=millis(),seconds=0;terminate==0;nonce++) {

		// apply midstate
		SHA256_Init(&ctx);
		memcpy(&ctx.h, midstate, 32);
		ctx.Nl = 512;

		// set nonce and hash the remaining bytes
		*(uint32_t*)(block_tail+12) = htonl(nonce);
		SHA256_Update(&ctx, block_tail, 16);
		SHA256_Final(hash, &ctx);

		// hash the result
		SHA256(hash, 32, hash);

		if (seconds>=timeout)
			break;

		if (hash[31]==0 && hash[30]==0 && hash[29]==0 && hash[28]==0 ) {
			terminate = 1;
			char hex[65];
			printf("< 0x%08x, hash: %s\n", nonce, btoh(hex, hash, 32));
			return nonce;
		}

		int end = millis();
		if (end-start>=1000) {
			seconds++;
			start = end;
			//printf("thread %d %ds %.2fMh/s\n", thread, seconds, (float)(nonce-start_nonce)/1000/1000/seconds);
			mhs[thread] = (float)(nonce-start_nonce)/1000/1000/seconds;
			float mh = .0f;
			if (thread==0) {
				for (int i=0; i<threads_num; i++)
					mh += mhs[i];
				printf("%ds %.2fMh/s\n", seconds, mh, threads_num);
			}
		}

		if (nonce==end_nonce)
			break;
	}
	return 0;
}

int double_hashing(uint8_t * block_header, uint32_t nonce, int check_midstate=0) {
	SHA256_CTX ctx;
	uint8_t hash[32];
	char hex[128];
	*(uint32_t*)(block_header+76) = htonl(nonce);
	printf("> 0x%08x, block: %s...\n", nonce, btoh(hex, block_header, 32-3));
	if (check_midstate) {
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, block_header, 64);
		printf("m 0x%08x, midstate: %s\n", nonce, btoh(hex, (uint8_t*)&ctx.h, 32));
		SHA256_Update(&ctx, block_header+64, 16);
		SHA256_Final(hash, &ctx);
	} else {
		SHA256(block_header, 80, hash);
	}
	SHA256(hash, 32, hash);
	printf("< 0x%08x, hash: %s\n", nonce, btoh(hex, hash, 32));
	return 0;
}

uint8_t payload_buffer[64];

static void * thread_func(void * tid) {
	uint32_t id = (int)tid;
	uint32_t range = 0xffffffff*id/threads_num;
	uint32_t start = range*id;
	ans[id] = find_nonce(payload_buffer, start, start+range-1, id);
}

int find_nonce_mt(uint8_t * payload) {
	terminate = 1;
	pthread_t threads[threads_num];
	memcpy(payload_buffer, payload, 64);
	for (int i=0; i<threads_num; i++)
		pthread_create ( threads + i, NULL, thread_func, (void*)i);
	for (int i=0; i<threads_num; i++)
		pthread_join ( threads[i], NULL );
	for(int i=0; i<threads_num; ++i)
		if (ans[i]!=0)
			return ans[i];
	return 0;
}


void tests() {
	printf("Running tests...\n");
	uint8_t buf[512];
	double_hashing(htob(buf, "0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c0101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff4d04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73ffffffff0100f2052a01000000434104678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5fac00000000"), 0x1dac2b7c);
	double_hashing(bufreverse(htob(buf, "0000000120c8222d0497a7ab44a1a2c7bf39de941c9970b1dc7cdc400000079700000000e88aabe1f353238c668d8a4df9318e614c10c474f8cdf8bc5f6397b946c33d7c4e7242c31a098ea500000000000000800000000000000000000000000000000000000000000000000000000000000000000000000000000080020000"),80), 0x063c5e01, 1);
	find_nonce(htob(buf, "2db907f9cb4eb938ded904f4832c43310380e3aeb54364057e7fec5157bfc5330000000000000000000000008000000000000000a58e091ac342724e7c3dc346"), 0x063c5e01);
	find_nonce(htob(buf, "ce92099c5a80bb81c52990d5c0924c625fd25a535640607d5a4bdf8174e2c8d500000000000000000000000080000000000000000b290c1a42313b4f21b5bcb8"), 0x8e0b31c5);
	find_nonce(htob(buf, "4679ba4ec99876bf4bfe086082b400254df6c356451471139a3afa71e48f544a000000000000000000000000000000000000000087320b1a1426674f2fa722ce"), 0x000187a2);
	for (uint32_t i=0, n[] = {0x04c0fdb4, 0x82540e46, 0x417c0f36, 0x60c994d5}; i<4; find_nonce(htob(buf, "2e4c8f91fd595d2d7ea20aaacb64a2a04382860277cf26b6a1ee04c56a5b504a00000000000000000000000000000000000000006461011ac906a951fb9b3c73"), n[i++]));
	//find_nonce_mt(htob(buf, "2e4c8f91fd595d2d7ea20aaacb64a2a04382860277cf26b6a1ee04c56a5b504a00000000000000000000000000000000000000006461011ac906a951fb9b3c73"));
}

#define golden_ob 0x4679ba4e
#define golden_nonce 0x000187a2
#define workdiv_sig 0x2e4c8f91
#define workdiv 0x04c0fdb4

void setup() {
	tests();
	Serial.begin(115200);
}

void reply(uint32_t data) {
	printf("sent: 0x%08x\n", data);
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
		case workdiv_sig: reply(workdiv); break;
		default: reply(find_nonce_mt(payload)); break;
	}
}

int main(int argc, char * argv[]) {
	if (argc>1) {
		sscanf(argv[1], "%d", &serial_port);
	} else {
		printf("Serial port is not specified, using %d\n", serial_port);
	}
	setup();
	for(;;) {
		loop();
		Sleep(1);
	}
}

