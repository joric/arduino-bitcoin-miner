# arduino-bitcoin-miner

Arduino Bitcoin Miner

## Video

[![Arduino Bitcoin Miner](http://img.youtube.com/vi/GMjrvpc9zDU/0.jpg)](https://www.youtube.com/watch?v=GMjrvpc9zDU)

## Usage

Build and upload sketch in Arduino IDE.
Arduino will work as a serial port Bitcoin miner (namely Icarus, device id ICA 0).
Run BFGMiner from the command line using testnet-in-a-box address and an Arduino COM port, e.g. for COM5 that would be:

`bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM5`

Or, for the mining pool (e.g. btc.com):

`bfgminer -o stratum+tcp://us.ss.btc.com:25 -u test.001 -p 123 -S icarus:\\.\COM5`

## Hashrate

Current hash speed is pretty abysmal, considering the 16 MHz Arduino Pro Micro (ATmega32U4 at 5v):

* ~ 50 hashes a second for arduino-bitcoin-miner.ino
* ~ 150 hashes a second for the AVR assembly version

At this rate you would need about an year to find a single share.
For the commercial [Cryptovia](http://cryptovia.com/cryptographic-libraries-for-avr-cpu/) library
(42744 cycles per 50 bytes) it would be roughly the same number of hashes (16MHz / 42744 / 2 ~ 187Hz), maybe even less.
All given values are for double hashing the 80-byte block header,
so every hash takes two 64-byte SHA256 blocks, consdering midstate optimization.

## What if

According to [Mining Profitability Calculator](https://www.cryptocompare.com/mining/calculator/), mining $1 a day needs 1.5 TH/s, and mining 1 BTC a year needs 42 TH/s, so:

* At 150 hashes a second per Arduino, mining one dollar a day would need 10 billion Arduinos
* Pro Micro consumes 200 mA, mining $1 a day with a 10 billion Arduino rig will need 2 gigawatts of power (Great Scott!)
* With an average price $0.2 per kWh, 2 gigawatt mining rig will cost you about $10M a day (minus one dollar you make)
* If you prefer a single AVR chip, mining 1 Bitcoin on ATmega32U4 will theoretically take about 280 billion years

## Emulator

There is also a PC version of the serial port miner (see pc_version directory).
You will need a serial port emulator, e.g. [com0com](https://code.google.com/archive/p/powersdr-iq/downloads).
It creates COM port pairs, e.g. you listen on COM8 and specify COM9 for the BFGMiner.
Emulator hash speed is currently about 1.14 million hashes a second (could be improved, maybe 6-7 million hashes per CPU core).

## Testnet-in-a-box

To debug solo mining on the localhost you'd need testnet-in-a-box.
Get the setup here: https://github.com/freewil/bitcoin-testnet-box.
There are two debug modes - testnet and regtest, edit configuration files and set testnet=1 or regtest=1 accordingly.

Testnet mode mining works with old bitcoin-core releases only.
I use bitcoin-qt 0.5.3.1 (coderrr mod with a coin control feature).
Get it here: https://luke.dashjr.org/~luke-jr/programs/bitcoin/files/bitcoind/coderrr/coincontrol/0.5.3.1/

Regtest (regression test mode) is a preferred method for debugging new bitcoin-core versions (I've used 0.16.0).
You need to download or generate at least 1 block to enable mining or you get RPC error 500 "Bitcoin is downloading blocks".
Either use "generate 1" in bitcoin-qt (Help - Debug Window - Console) or use a Python script to run commands via RPC:

```
import requests
requests.post('http://admin1:123@localhost:19001', data='{"method": "generate", "params": [1]}')
```

Both testnet and regtest work well with cpuminer (and it supports fallback from getblocktemplate to getwork for old clients):

`minerd -a sha256d -o http://localhost:19001 -O admin1:123 --coinbase-addr=<solo mining address>`

CGMiner 3.7.2 also supports testnet and getwork, use --gpu-platform 1 for laptop nvidia cards (1030 gives about 200 Mh/s):

`cgminer -o http://localhost:19001 -O admin1:123 --gpu-platform 1`

## Protocol

This miner uses Icarus protocol via USB serial port emulation (there is no USB autodetection, you have to specify a COM port).

### Icarus

* No detection is needed (no special command for this).
* Wait for data. Each data packet is 64 bytes: 32 bytes midstate + 20 fill bytes (any value) + last 12 bytes of block header.
* Send back the results, i.e. when valid share is found, send back the 4-byte result nonce immediately.

Read more about the protocol here: http://en.qi-hardware.com/wiki/Icarus#Communication_protocol_V3

#### BFGMiner specific

* BFGMiner tests block with nonce 0x000187a2 first. Since we need to test only 100258 values the reply should be instant.
* Sends work division payload 2e4c8f91(...). Expects 0x04c0fdb4, 0x82540e46, 0x417c0f36, 0x60c994d5 for 1, 2, 4, 8 cores.
* If no data sent back in ~11.3 seconds (full cover time on 32bit range at 380MH/s FPGA), miner sends another work.

#### USB autodetection

Not implemented yet, you have to specify a COM port in the command line.
Original Icarus uses either VID_067B & PID_2303 (USBDeviceShare) or VID_1FC9 & PID_0083 (LPC USB VCom Port driver).
Default Arduino Leonardo driver uses VID_2341 & PID_8036, and neither BFGMiner nor CGMiner recognize it as an USB mining device.
Changing hardware ID's requires updating bootloader and fixing the driver.


## Algorithms

### Sha256d

As the majority of Bitcoin miners, this one also uses midstate hashing optimization.
Midstate is a 32-byte long data string,
a part of the hashing function context after processing the first 64 bytes of the block header.
Simply apply the state from the payload, process the remaining 16 (80-64) bytes of the block header
(including nonce in the end), and hash the result.

```c
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
```

### Scrypt

Scrypt is not supported (yet). ATmega32U4 only has 2.5K RAM, so even basic Scrypt-based cryptocurrencies are not feasible,
for example, Litecoin uses Scrypt with 128 KB RAM (in order to fit into a typical L2 cache).
Other cryptocurrencies have even higher memory requirements, e.g. CryptoNight algorithm used in Monero
requires at least a megabyte of internal memory.

## References

* https://hackaday.com/2018/04/22/hackaday-links-april-22-2018/
* https://www.reddit.com/r/arduino/comments/8dshqd/arduino_pro_microbased_usb_bitcoin_miner_150_hs/
* https://www.quora.com/How-long-would-it-take-to-mine-a-Bitcoin-on-an-AVR-chip-Arduino
* https://linustechtips.com/main/topic/840241-arduino-bitcoin-miner/


