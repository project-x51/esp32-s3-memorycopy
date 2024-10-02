# esp32-s3-memorycopy
Benchmarks the various approaches that can be taken to moving data around between internal and external memories on an ESP32-S3. 
Written for the ESP32-S3-WROOM-1U-N8R8 and requires the esp-dsp component to be present as well.
Can be compiled with or without evicting cache. Accurate performance requires using a cache flush to ensure all data is transferred when the test ends.

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

### Results
A Google sheet of the results is available
[here](https://docs.google.com/spreadsheets/d/1A9UKdOb0QqLGQVSIru1gydPLEyCpcejhV0q_KI-OJMs/edit?usp=sharing).

![image](https://github.com/user-attachments/assets/c5074376-4fe7-423b-82ba-9034ae7fcf0d)


Most recent log below...

```
memory copy version 1.3

I (373) Memory Copy: Using PSRAM CACHE flush

I (383) Memory Copy: Allocating 2 x 100kb in IRAM, alignment: 32 bytes
I (443) Memory Copy: 8-bit for loop copy IRAM->IRAM took 512013 CPU cycles = 45.78 MB/s
I (493) Memory Copy: 16-bit for loop copy IRAM->IRAM took 204815 CPU cycles = 114.43 MB/s
I (543) Memory Copy: 32-bit for loop copy IRAM->IRAM took 102415 CPU cycles = 228.85 MB/s
I (593) Memory Copy: 64-bit for loop copy IRAM->IRAM took 76816 CPU cycles = 305.11 MB/s
I (643) Memory Copy: memcpy IRAM->IRAM took 64039 CPU cycles = 365.99 MB/s
I (693) Memory Copy: async_memcpy IRAM->IRAM took 407950 CPU cycles = 57.45 MB/s
I (743) Memory Copy: PIE 128-bit (16 byte loop) IRAM->IRAM took 19408 CPU cycles = 1207.62 MB/s
I (793) Memory Copy: PIE 128-bit (32 byte loop) IRAM->IRAM took 12809 CPU cycles = 1829.77 MB/s
I (843) Memory Copy: DSP AES3 IRAM->IRAM took 16221 CPU cycles = 1444.89 MB/s

I (893) Memory Copy: Freeing 100kb from IRAM
I (893) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (943) Memory Copy: 8-bit for loop copy IRAM->PSRAM took 864094 CPU cycles = 27.12 MB/s
I (993) Memory Copy: 16-bit for loop copy IRAM->PSRAM took 736145 CPU cycles = 31.84 MB/s
I (1043) Memory Copy: 32-bit for loop copy IRAM->PSRAM took 717694 CPU cycles = 32.66 MB/s
I (1093) Memory Copy: 64-bit for loop copy IRAM->PSRAM took 717699 CPU cycles = 32.66 MB/s
I (1143) Memory Copy: memcpy IRAM->PSRAM took 720811 CPU cycles = 32.52 MB/s
I (1243) Memory Copy: PIE 128-bit (16 byte loop) IRAM->PSRAM took 720756 CPU cycles = 32.52 MB/s
I (1293) Memory Copy: PIE 128-bit (32 byte loop) IRAM->PSRAM took 720757 CPU cycles = 32.52 MB/s
I (1343) Memory Copy: DSP AES3 IRAM->PSRAM took 724166 CPU cycles = 32.36 MB/s

I (1393) Memory Copy: Swapping source and destination buffers
I (1443) Memory Copy: 8-bit for loop copy PSRAM->IRAM took 796809 CPU cycles = 29.41 MB/s
I (1493) Memory Copy: 16-bit for loop copy PSRAM->IRAM took 460818 CPU cycles = 50.86 MB/s
I (1543) Memory Copy: 32-bit for loop copy PSRAM->IRAM took 403215 CPU cycles = 58.13 MB/s
I (1593) Memory Copy: 64-bit for loop copy PSRAM->IRAM took 412814 CPU cycles = 56.77 MB/s
I (1643) Memory Copy: memcpy PSRAM->IRAM took 412838 CPU cycles = 56.77 MB/s
I (1693) Memory Copy: async_memcpy PSRAM->IRAM took 445348 CPU cycles = 52.63 MB/s
I (1743) Memory Copy: PIE 128-bit (16 byte loop) PSRAM->IRAM took 403209 CPU cycles = 58.13 MB/s
I (1793) Memory Copy: PIE 128-bit (32 byte loop) PSRAM->IRAM took 403207 CPU cycles = 58.13 MB/s
I (1843) Memory Copy: DSP AES3 PSRAM->IRAM took 406622 CPU cycles = 57.64 MB/s

I (1893) Memory Copy: Freeing 100kb from IRAM
I (1893) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (1953) Memory Copy: 8-bit for loop copy PSRAM->PSRAM took 1200869 CPU cycles = 19.52 MB/s
I (2013) Memory Copy: 16-bit for loop copy PSRAM->PSRAM took 1109766 CPU cycles = 21.12 MB/s
I (2073) Memory Copy: 32-bit for loop copy PSRAM->PSRAM took 1103614 CPU cycles = 21.24 MB/s
I (2133) Memory Copy: 64-bit for loop copy PSRAM->PSRAM took 1103616 CPU cycles = 21.24 MB/s
I (2193) Memory Copy: memcpy PSRAM->PSRAM took 1103628 CPU cycles = 21.24 MB/s
I (2253) Memory Copy: async_memcpy PSRAM->PSRAM took 887909 CPU cycles = 26.40 MB/s
I (2313) Memory Copy: PIE 128-bit (16 byte loop) PSRAM->PSRAM took 1113184 CPU cycles = 21.05 MB/s
I (2373) Memory Copy: PIE 128-bit (32 byte loop) PSRAM->PSRAM took 1111652 CPU cycles = 21.08 MB/s
I (2433) Memory Copy: DSP AES3 PSRAM->PSRAM took 1115058 CPU cycles = 21.02 MB/s
I (2483) main_task: Returned from app_main()
```

Thanks to Microcontroller and X-Ryl669 for the hard stuff!
+ [Original Thread](https://www.esp32.com/viewtopic.php?f=13&t=36808&sid=9af3e432eb4843d96437bc07485415c8)
+ [Suggestions](https://github.com/project-x51/esp32-s3-memorycopy/issues/2)


# Version Tracking
## Version 1.3
Fixed the order of source and destination so results properly reflect performance. Fixed cache terminology.

## Version 1.2
Added code to flush the PSRAM cache before and after running the tests.

## Version 1.1
Added IRAM_ATTR to time sensitive code blocks so they are in IRAM avoiding need to load them into instruction cache.

## Version 1.0
Original output. Known as the baseline.

