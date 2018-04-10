# arduino-bitcoin-miner

Arduino Bitcoin Miner

[![Arduino Bitcoin Miner](http://img.youtube.com/vi/GMjrvpc9zDU/0.jpg)](https://www.youtube.com/watch?v=GMjrvpc9zDU)

## Usage

### Arduino

Build and upload sketch in Arduino IDE.
Arduino will work as a serial port Bitcoin miner (namely Icarus, device id ICA 0).
You have to specify Arduino COM port for the bfgminer, e.g. for COM5 that would be:

`bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM5`

Hash speed is pretty abysmal, about 50 hashes a second on Arduino Pro Micro.

### PC emulator

There is also some test code for a hardware miner emulator on a PC
(see [icarus_emul](https://github.com/joric/arduino-bitcoin-miner/tree/master/icarus_emul) directory).
You will a need serial port emulator, I use [com0com](https://code.google.com/archive/p/powersdr-iq/downloads).
It creates COM port pairs, e.g. COM8-COM9 means you work with COM8 and use COM9 in bfgminer.
Hash speed is about 1.14 million hashes a second (could be improved, maybe 6-7 million hashes per CPU core).

## Bitcoin-in-a-box

Get the setup here: https://github.com/freewil/bitcoin-testnet-box.
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

## USB Autodetection

USB autodetection is not implemented yet. Default Arduino Leonardo driver uses VID_2341 & PID_8036,
and neither bfgminer nor cgminer recognize it as an USB mining device.
Original ICA uses either VID_067B & PID_2303 (USBDeviceShare) or VID_1FC9 & PID_0083 (LPC USB VCom Port driver).
Changing hardware ids probably requires updating MCU bootloader and editing the driver.

## Midstate hashing optimization

Most hardware miners use midstate hashing optimization.
This is pretty simple: midstate is a SHA256 32-bit state (intermediate digest state) after processing the first 64 bytes of the block header.
Load the state, process the remaining 16 (80-64) bytes (including nonce in the end),
get the result, double hash it as usual and you're done. This code is pretty self-explanatory:

```
SHA256_CTX ctx;

// apply midstate
memcpy(&ctx.state, midstate, 32);
ctx.datalen = 0;
ctx.bitlen = 512;

// set nonce and hash the remaining bytes
*(uint32_t*)(block_tail+12) = htonl(nonce);
sha256_update(&ctx, block_tail, 16);
sha256_final(&ctx, hash);

// double hash the result
sha256_init(&ctx);
sha256_update(&ctx, hash, 32);
sha256_final(&ctx, hash);
```

## References

* Bitcoin Core releases: https://bitcoin.org/bin



