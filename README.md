# esp32-s3-memorycopy
Benchmarks the various approaches that can be taken to moving data around between internal and external memories on an ESP32-S3. 
Written for the ESP32-S3-WROOM-1U-N8R8 and requires the esp-dsp component to be present as well.

### Currently supports moving data between...
+ IRAM->IRAM
+ IRAM->PSRAM
+ PSRAM->IRAM
+ PSRAM-.PSRAM

### Uses the following methods to move data...

+ For loop (8 bit, 16 bit, 32 bit & 64 bit)
+ Memcpy
+ Async_memcpy
+ 128-bit ESP32-S3 PIE SIMD Extensions
+ ESP-DSP component's dsps_memcpy_aes3 function

Log below...


```
I (394) Memory Copy:

memory copy version 1.

I (404) Memory Copy: Allocating 2 x 100kb in IRAM, alignment: 32 bytes
I (464) Memory Copy: 8-bit for loop copy IRAM->IRAM took 819922 CPU cycles = 28.59 MB/s
I (514) Memory Copy: 16-bit for loop copy IRAM->IRAM took 205776 CPU cycles = 113.90 MB/s
I (564) Memory Copy: 32-bit for loop copy IRAM->IRAM took 103383 CPU cycles = 226.71 MB/s
I (614) Memory Copy: 64-bit for loop copy IRAM->IRAM took 77682 CPU cycles = 301.71 MB/s
I (664) Memory Copy: memcpy IRAM->IRAM took 64323 CPU cycles = 364.37 MB/s
I (714) Memory Copy: async_memcpy IRAM->IRAM took 408520 CPU cycles = 57.37 MB/s
I (764) Memory Copy: PIE 128-bit (16 byte loop) IRAM->IRAM took 19498 CPU cycles = 1202.05 MB/s
I (814) Memory Copy: PIE 128-bit (32 byte loop) IRAM->IRAM took 13095 CPU cycles = 1789.81 MB/s
I (864) Memory Copy: DSP AES3 IRAM->IRAM took 15813 CPU cycles = 1482.17 MB/s

I (914) Memory Copy: Freeing 100kb from IRAM
I (914) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (964) Memory Copy: 8-bit for loop copy IRAM->PSRAM took 1075498 CPU cycles = 21.79 MB/s
I (1014) Memory Copy: 16-bit for loop copy IRAM->PSRAM took 461778 CPU cycles = 50.75 MB/s
I (1064) Memory Copy: 32-bit for loop copy IRAM->PSRAM took 404325 CPU cycles = 57.97 MB/s
I (1114) Memory Copy: 64-bit for loop copy IRAM->PSRAM took 413871 CPU cycles = 56.63 MB/s
I (1164) Memory Copy: memcpy IRAM->PSRAM took 413294 CPU cycles = 56.71 MB/s
I (1214) Memory Copy: async_memcpy IRAM->PSRAM took 465457 CPU cycles = 50.35 MB/s
I (1264) Memory Copy: PIE 128-bit (16 byte loop) IRAM->PSRAM took 403440 CPU cycles = 58.09 MB/s
I (1314) Memory Copy: PIE 128-bit (32 byte loop) IRAM->PSRAM took 403638 CPU cycles = 58.07 MB/s
I (1364) Memory Copy: DSP AES3 IRAM->PSRAM took 405830 CPU cycles = 57.75 MB/s

I (1414) Memory Copy: Swapping source and destination buffers
I (1464) Memory Copy: 8-bit for loop copy PSRAM->IRAM took 1037131 CPU cycles = 22.60 MB/s
I (1514) Memory Copy: 16-bit for loop copy PSRAM->IRAM took 621710 CPU cycles = 37.70 MB/s
I (1564) Memory Copy: 32-bit for loop copy PSRAM->IRAM took 603621 CPU cycles = 38.83 MB/s
I (1614) Memory Copy: 64-bit for loop copy PSRAM->IRAM took 603466 CPU cycles = 38.84 MB/s
I (1664) Memory Copy: memcpy PSRAM->IRAM took 605957 CPU cycles = 38.68 MB/s
I (1714) Memory Copy: async_memcpy PSRAM->IRAM took 447733 CPU cycles = 52.35 MB/s
I (1764) Memory Copy: PIE 128-bit (16 byte loop) PSRAM->IRAM took 605494 CPU cycles = 38.71 MB/s
I (1814) Memory Copy: PIE 128-bit (32 byte loop) PSRAM->IRAM took 605790 CPU cycles = 38.69 MB/s
I (1864) Memory Copy: DSP AES3 PSRAM->IRAM took 607982 CPU cycles = 38.55 MB/s

I (1914) Memory Copy: Freeing 100kb from IRAM
I (1914) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (1974) Memory Copy: 8-bit for loop copy PSRAM->PSRAM took 1412578 CPU cycles = 16.59 MB/s
I (2034) Memory Copy: 16-bit for loop copy PSRAM->PSRAM took 1052370 CPU cycles = 22.27 MB/s
I (2094) Memory Copy: 32-bit for loop copy PSRAM->PSRAM took 1046370 CPU cycles = 22.40 MB/s
I (2154) Memory Copy: 64-bit for loop copy PSRAM->PSRAM took 1046215 CPU cycles = 22.40 MB/s
I (2214) Memory Copy: memcpy PSRAM->PSRAM took 1045637 CPU cycles = 22.41 MB/s
I (2274) Memory Copy: async_memcpy PSRAM->PSRAM took 887275 CPU cycles = 26.42 MB/s
I (2334) Memory Copy: PIE 128-bit (16 byte loop) PSRAM->PSRAM took 1054866 CPU cycles = 22.22 MB/s
I (2394) Memory Copy: PIE 128-bit (32 byte loop) PSRAM->PSRAM took 1053534 CPU cycles = 22.25 MB/s
I (2454) Memory Copy: DSP AES3 PSRAM->PSRAM took 1055945 CPU cycles = 22.20 MB/s
I (2504) main_task: Returned from app_main()
```

Thanks to Microcontroller for the hard stuff!
https://www.esp32.com/viewtopic.php?f=13&t=36808&sid=9af3e432eb4843d96437bc07485415c8
