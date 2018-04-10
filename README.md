# arduino-bitcoin-miner

Arduino Bitcoin Miner

![arduino-bitcoin-miner](https://i.imgur.com/tzfPink.jpg)

## Usage

### Arduino


Build and upload sketch in Arduino IDE.

Start bfgminer using Arduino COM port, e.g. for bitcoin-in-a-box and COM5:

`bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM5`

Arduino will work as an USB Bitcoin Miner (namely Icarus, device id ICA 0).

### PC only

There is also some test code for a hardware miner emulator on a PC (see icarus_emul directory).

You'd need serial port emulator for debugging on PC: https://code.google.com/archive/p/powersdr-iq/downloads

By default it creates COM port pairs, e.g. COM8-COM9 means you work with COM8 and use COM9 in bfgminer.

`bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM9`

## Debugging

### Bitcoin-in-a-box

Get the setup here: https://github.com/freewil/bitcoin-testnet-box

There are two debug modes - testnet and regtest, edit configuration files and set testnet=1 or regtest=1 accordingly.

#### Testnet mode

Testnet-in-a-box works with old bitcoin-core releases only.
I use bitcoin-qt 0.5.3.1 (coderrr mod with a coin control feature).
You can download it here: https://luke.dashjr.org/~luke-jr/programs/bitcoin/files/bitcoind/coderrr/coincontrol/0.5.3.1/

#### Regtest mode

Regtest (regression test mode) is a preferred method for debugging new bitcoin-core versions (I've used 0.16.0).
You need to download or generate at least 1 block to enable mining or you get RPC error 500 "Bitcoin is downloading blocks".
Either use "generate 1" in bitcoin-qt (Help - Debug Window - Console) or use a Python script to run commands via RPC:

```
import requests
requests.post('http://admin1:123@localhost:19001', data='{"method": "generate", "params": [1]}')
```

Both testnet and regtest work with cpuminer (it also supports fallback from getblocktemplate to getwork for old clients):

`minerd -a sha256d -o http://localhost:19001 -O admin1:123 --coinbase-addr=<solo mining address>`

## Communication protocol

Most miners are icarus-based, should work for all stm32 and avr miners that use USB serial port emulation (e.g. via Zadig).

Read about the protocol here: http://en.qi-hardware.com/wiki/Icarus#Communication_protocol_V3

## Midstate

Some people experience a problem with midstate hashing.
This is really very simple. Midstate is SHA256 state after hashing the first 64 bytes of the block header.
You save it for later, apply it in the very beginning and feed the remaining 16 (80-64) bytes (including nonce in the end).
Then you get the digest and double hash it as usual. This code is pretty self-explanatory:

```
...
SHA256_CTX ctx;

// set midstate
sha256_init(&ctx);
memcpy(&ctx.state, midstate, 32);
ctx.datalen = 0;
ctx.bitlen = 512;

// set nonce and hash the remaining bytes
*(uint32_t*)(block_tail+12) = htonl(nonce);
sha256_update(&ctx, block_tail, 16);
sha256_final(&ctx, hash);

// finally, double hashing
sha256_init(&ctx);
sha256_update(&ctx, hash, 32);
sha256_final(&ctx, hash);
...

```

## References

* Bitcoin Core releases: https://bitcoin.org/bin



