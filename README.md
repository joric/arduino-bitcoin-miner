# arduino-bitcoin-miner

Koparka Bitcoin z Arduino
**UWAGA: Jedyne, co zrobiłem to przetłumaczyłem teksty. [ORYGINAŁ TUTAJ/ORIGINAL HERE](https://github.com/joric/arduino-bitcoin-miner)**

## Film

[![Koparka Bitcoin z Arduino](http://img.youtube.com/vi/GMjrvpc9zDU/0.jpg)](https://www.youtube.com/watch?v=GMjrvpc9zDU)

## Przydatne programy

* [BFGMiner 4.10.6](http://bfgminer.org/files/4.10.6/bfgminer-4.10.6-win64.zip)
* [CGMiner 3.7.2](https://cryptomining-blog.com/wp-content/download/cgminer-3.7.2-windows.rar)
* [CPUMiner 2.5.0](https://github.com/pooler/cpuminer/releases/download/v2.5.0/pooler-cpuminer-2.5.0-win64.zip)
* [Emulator portu szeregowego](https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/powersdr-iq/setup_com0com_W7_x64_signed.exe)
* [Arduino IDE](https://www.arduino.cc/download_handler.php?f=/arduino-1.8.5-windows.exe)

## Arduino

Kod działa na większości Arduino, np:
* [Arduino Pro Micro](https://www.aliexpress.com/item//32846843498.html)

# Kopanie bitcoinów

Wgraj sketch do arduino przez Arduino IDE.
Arduino zadziała jako szeregowa koparka Bitcoin (nazwowo Icarus, id urządzenia ICA 0).
Włącz BFGminer z lini poleceń (cmd) używając adresu testnet-in-a-box i Portu COM Arduino, np. dla COM5:

`bfgminer -o http://localhost:19001 -u admin1 -p 123 -S icarus:\\.\COM5`

Lub, dla kopania w pool'u (np. btc.com):

`bfgminer -o stratum+tcp://us.ss.btc.com:25 -u test.001 -p 123 -S icarus:\\.\COM5`

# Hashrate

Póki co, na Arduino Pro Micro udało mi się wyciągnąć:

* ~ 50 hash na sekundę przez *arduino-bitcoin-miner.ino*
* ~ 150 hash na sekundę przez *AVR assembly version*

Arduino musi działać przez ~rok by znaleźć jeden share.
Dla komercyjnej biblioteki [Cryptovia](http://cryptovia.com/cryptographic-libraries-for-avr-cpu/)
(42744 cykli na 50 bajtów) będzie taka sama liczba hashy (16MHz / 42744 / 2 ~ 187Hz), a nawet mniej.
Podane wartości są dla 80-znakowego nagłówka,
więc dwa 64-bajtowe bloki SHA256, biorąc pod uwagę optymalizację *midstate*.

## Co jeśli

Według [Mining Profitability Calculator](https://www.cryptocompare.com/mining/calculator/), kopanie $1 na dzień wymaga 1.5 TH/s, 1 BTC na rok wymaga 42 TH/s (około).

* Przy 150 hashy na sekundę na jedno Arduino, kopanie dolara dziennie wymagałoby 10 miliardów płytek Arduino (!).
* Pro Micro pobiera 200 mA, kopanie $1 na dzień z 10 miliardowym sprzętem Arduino potrzebowałoby 2 GigaWaty energii. (Great Scott!)
* Przy cenie $0.2 za kWh, 2 gigawaty kosztowałyby cię $10M na dzień (minus $1 który zarobisz)
* Jeżeli wolisz jeden chip AVR, wykopanie 1 Bitcoina na ATmega32U4 teoretycznie zajmie 280 billion years (w najgorszym wypadku).

## Emulator

Jest też wersja na PC (sprawdź folder *pc_version*).
Będziesz potrzebował emulatora portu szeregowego, np. [com0com](https://code.google.com/archive/p/powersdr-iq/downloads).
Tworzy pary portów COM, np. nasłuchuje COM8 i wybiera COM9 dla BFGMiner'a.
Szybkość emulatora to 1.14 milionów hashy na sekundę (mógłbym to poprawić, może 6-7 milionów hashy na rdzeń).

## Testnet-in-a-box

Aby debugować koparkę będziesz potrzebował *testnet-in-a-box*.
Pomocy szukaj tu: https://github.com/freewil/bitcoin-testnet-box.
Są dwa tryby debugowania - testnet i regtest, edytuj pliki konfiguracyjne i ustaw testnet=1 lub regtest=1 według potrzeb.

Kopanie w trybie testnet działa tylko dla starych trybów bitcoina.
Używam bitcoin-qt 0.5.3.1 (mod coderrr'a z kontrolą monety).
Pobierz go stąd: https://luke.dashjr.org/~luke-jr/programs/bitcoin/files/bitcoind/coderrr/coincontrol/0.5.3.1/

Regtest (regresywny tryb testowania) jest dobry do debugowania nowych wersji bitcoin-core (Ja używałem 0.16.0).
Musisz wygenerować conajmniej 1 blok by włączyć kopanie, inaczej dostaniesz błąd: RPC error 500 "Bitcoin is downloading blocks".
Lub uźyj "generate 1" w bitcoin-qt (Help (pomoc) - Debug Window (okno debugowania) - Console (console)) albo użyj kody Python do uruchomienia skryptu przez RPC:

```
import requests
requests.post('http://admin1:123@localhost:19001', data='{"method": "generate", "params": [1]}')
```

Testnet i regtest działają dobrze z cgminer. (działa fallback z getblocktemplate by działały stare klienty):

`minerd -a sha256d -o http://localhost:19001 -O admin1:123 --coinbase-addr=<solo mining address>`

CGMiner 3.7.2 wspiera testnet i getwork, użyj --gpu-platform 1 dla laptopowych kart NVidi'i (1030 daje około 200 Mh/s):

`cgminer -o http://localhost:19001 -O admin1:123 --gpu-platform 1`

## Protokół

Moja wersja używa protokołu Icarus przez emulację USB (nie ma auto-wykrywania USB więc musisz samemu podać port COM).

### Icarus

* Nie jest wymagana detekcja (nie trzeba dodatkowych komend).
* Czekanie na dane. Każdy pakiet danych to 64 bajty: 32 bajty midstate + 20 bajtów wypełniających (jakiekolwiek dane) + ostatnie 12 bajtów z nagłówka.
* Wysłanie z powrotem rezultatów, np. kiedy Arduino znajdzie share, wyślij 4-bajtowy wynik od razu.

Możesz poczytać więcej o protokole Icarus tutaj: http://en.qi-hardware.com/wiki/Icarus#Communication_protocol_V3

#### dla BFGminer:

* BFGMiner testuje najpierw bloki 0x000187a2. Przez to, że musimy pretestować 100258 wartości odpowiedź powinna być wręcz natychmiastowa.
* Wysyła ładunek podziału pracy 2e4c8f91(...). Spodziewa się 0x04c0fdb4, 0x82540e46, 0x417c0f36, 0x60c994d5 dla 1, 2, 4, 8 rdzeni.
* Jeżeli nie wysłane zostały dane w ~11.3 sekund (pełny czas na koparkach 32bit i 380MH/s FPGA), koparka wysyła kolejną pracę.

#### autowykrywanie USB

Jeszcze nie jest zaimplementowane, musisz podać port COM w lini poleceń (cmd).
Oryginalne Icarus'y używają VID_067B & PID_2303 (USBDeviceShare) lub VID_1FC9 & PID_0083 (sterownik portu LPC USB VCom).
Zwyczajny sterownik Arduino Leonardo używa VID_2341 & PID_8036, i ani BFGMiner i CGMiner nie wykryją go jako koparkę.
Zmiana ID w sterownikach wymaga flashowaia nowego firmware.


## Algorytmy

### Sha256d

Jak większość koparek Bitcoin, ten też ma optymalizację *midstate*.
*Midstate* to 32-bajtowy ciąg danych,
część kontekstu funkcji *midstate* po przetworzeniu pierwszych 64 bajtów nagłówka bloku.
Po prostu pobierz stan z koparki, przetwórz pozostałe 16 (80-64) bajty nagłówka bloku
(w tym nonce na końcu) i wyślij wynik.

```c
// dodaj midstate
SHA256_Init(&ctx);
memcpy(&ctx.h, midstate, 32);
ctx.Nl = 512;

// wyślij nonce i pozostale bajty
*(uint32_t*)(block_tail+12) = htonl(nonce);
SHA256_Update(&ctx, block_tail, 16);
SHA256_Final(hash, &ctx);

// wyślij wynik
SHA256(hash, 32, hash);
```

### Scrypt

Scrypt nie jest zaimplementowany (jeszcze). ATmega32U4 ma 2.5K RAMu, więc nawet proste bazowane na Scrypt kryptowaluty nie nadają się,
na przykład, Litecoin używa Scrypt z 128 KB RAM (żeby zmieścić się w typowe 128KB cache L2 procesora).
Inne kryptowaluty mają jeszcze wyższe wymagania, np. algorytm CryptoNight używany w Monero
potrzebuje conajmniej megabajt pamieci.

## Odnośniki

* https://hackaday.com/2018/04/22/hackaday-links-april-22-2018/
* https://www.reddit.com/r/arduino/comments/8dshqd/arduino_pro_microbased_usb_bitcoin_miner_150_hs/
* https://www.quora.com/How-long-would-it-take-to-mine-a-Bitcoin-on-an-AVR-chip-Arduino
* https://linustechtips.com/main/topic/840241-arduino-bitcoin-miner/


