# esp32-s3-memorycopy
Benchmarks the various approaches that can be taken to moving data around between internal and external memories on an ESP32-S3. 
Written for the ESP32-S3-WROOM-1U-N8R8

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
I (391) Memory Copy:

memory copy version 1.

I (401) Memory Copy: Allocating 2 x 100kb in IRAM, alignment: 32 bytes
I (421) Memory Copy: 8-bit for loop copy IRAM->IRAM took 819662 CPU cycles = 28.59 MB/s
I (421) Memory Copy: 16-bit for loop copy IRAM->IRAM took 461530 CPU cycles = 50.78 MB/s
I (431) Memory Copy: 32-bit for loop copy IRAM->IRAM took 206343 CPU cycles = 113.59 MB/s
I (441) Memory Copy: 64-bit for loop copy IRAM->IRAM took 128300 CPU cycles = 182.68 MB/s
I (451) Memory Copy: memcpy IRAM->IRAM took 64658 CPU cycles = 362.48 MB/s
I (461) Memory Copy: async_memcpy IRAM->IRAM took 412233 CPU cycles = 56.85 MB/s
I (461) Memory Copy: PIE 128-bit IRAM->IRAM took 13145 CPU cycles = 1783.00 MB/s
I (471) Memory Copy: DSP AES3 IRAM->IRAM took 17028 CPU cycles = 1376.41 MB/s

I (471) Memory Copy: Freeing 100kb from IRAM
I (481) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (501) Memory Copy: 8-bit for loop copy IRAM->PSRAM took 1075776 CPU cycles = 21.79 MB/s
I (501) Memory Copy: 16-bit for loop copy IRAM->PSRAM took 720958 CPU cycles = 32.51 MB/s
I (511) Memory Copy: 32-bit for loop copy IRAM->PSRAM took 462534 CPU cycles = 50.67 MB/s
I (521) Memory Copy: 64-bit for loop copy IRAM->PSRAM took 422886 CPU cycles = 55.42 MB/s
I (531) Memory Copy: memcpy IRAM->PSRAM took 413299 CPU cycles = 56.71 MB/s
I (541) Memory Copy: async_memcpy IRAM->PSRAM took 478345 CPU cycles = 49.00 MB/s
I (541) Memory Copy: PIE 128-bit IRAM->PSRAM took 403536 CPU cycles = 58.08 MB/s
I (551) Memory Copy: DSP AES3 IRAM->PSRAM took 409011 CPU cycles = 57.30 MB/s

I (551) Memory Copy: Swapping source and destination buffers
I (571) Memory Copy: 8-bit for loop copy PSRAM->IRAM took 1040185 CPU cycles = 22.53 MB/s
I (581) Memory Copy: 16-bit for loop copy PSRAM->IRAM took 696069 CPU cycles = 33.67 MB/s
I (591) Memory Copy: 32-bit for loop copy PSRAM->IRAM took 615118 CPU cycles = 38.10 MB/s
I (591) Memory Copy: 64-bit for loop copy PSRAM->IRAM took 602883 CPU cycles = 38.88 MB/s
I (601) Memory Copy: memcpy PSRAM->IRAM took 607278 CPU cycles = 38.59 MB/s
I (611) Memory Copy: async_memcpy PSRAM->IRAM took 458985 CPU cycles = 51.06 MB/s
I (621) Memory Copy: PIE 128-bit PSRAM->IRAM took 605593 CPU cycles = 38.70 MB/s
I (631) Memory Copy: DSP AES3 PSRAM->IRAM took 611055 CPU cycles = 38.36 MB/s

I (631) Memory Copy: Freeing 100kb from IRAM
I (631) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (651) Memory Copy: 8-bit for loop copy PSRAM->PSRAM took 1422563 CPU cycles = 16.48 MB/s
I (661) Memory Copy: 16-bit for loop copy PSRAM->PSRAM took 1090496 CPU cycles = 21.49 MB/s
I (681) Memory Copy: 32-bit for loop copy PSRAM->PSRAM took 1049829 CPU cycles = 22.33 MB/s
I (691) Memory Copy: 64-bit for loop copy PSRAM->PSRAM took 1049128 CPU cycles = 22.34 MB/s
I (701) Memory Copy: memcpy PSRAM->PSRAM took 1049045 CPU cycles = 22.34 MB/s
I (711) Memory Copy: async_memcpy PSRAM->PSRAM took 895774 CPU cycles = 26.16 MB/s
I (721) Memory Copy: PIE 128-bit PSRAM->PSRAM took 1054546 CPU cycles = 22.23 MB/s
I (741) Memory Copy: DSP AES3 PSRAM->PSRAM took 1059057 CPU cycles = 22.13 MB/s
I (741) main_task: Returned from app_main()
```

Thanks to Microcontroller for the hard stuff!
https://www.esp32.com/viewtopic.php?f=13&t=36808&sid=9af3e432eb4843d96437bc07485415c8
