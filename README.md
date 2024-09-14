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

### Results
A Google sheet of the results is available
[here](https://docs.google.com/spreadsheets/d/1A9UKdOb0QqLGQVSIru1gydPLEyCpcejhV0q_KI-OJMs/edit?usp=sharing).

Most recent log below...

```
memory copy version 1.1

I (406) Memory Copy: Allocating 2 x 100kb in IRAM, alignment: 32 bytes
I (466) Memory Copy: 8-bit for loop copy IRAM->IRAM took 819210 CPU cycles = 28.61 MB/s
I (516) Memory Copy: 16-bit for loop copy IRAM->IRAM took 204816 CPU cycles = 114.43 MB/s
I (566) Memory Copy: 32-bit for loop copy IRAM->IRAM took 102416 CPU cycles = 228.85 MB/s
I (616) Memory Copy: 64-bit for loop copy IRAM->IRAM took 76816 CPU cycles = 305.11 MB/s
I (666) Memory Copy: memcpy IRAM->IRAM took 64035 CPU cycles = 366.01 MB/s
I (716) Memory Copy: async_memcpy IRAM->IRAM took 407422 CPU cycles = 57.53 MB/s
I (766) Memory Copy: PIE 128-bit (16 byte loop) IRAM->IRAM took 19405 CPU cycles = 1207.81 MB/s
I (816) Memory Copy: PIE 128-bit (32 byte loop) IRAM->IRAM took 12806 CPU cycles = 1830.20 MB/s
I (866) Memory Copy: DSP AES3 IRAM->IRAM took 16033 CPU cycles = 1461.83 MB/s

I (916) Memory Copy: Freeing 100kb from IRAM
I (916) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (966) Memory Copy: 8-bit for loop copy IRAM->PSRAM took 1075492 CPU cycles = 21.79 MB/s
I (1016) Memory Copy: 16-bit for loop copy IRAM->PSRAM took 460990 CPU cycles = 50.84 MB/s
I (1066) Memory Copy: 32-bit for loop copy IRAM->PSRAM took 403291 CPU cycles = 58.12 MB/s
I (1116) Memory Copy: 64-bit for loop copy IRAM->PSRAM took 412814 CPU cycles = 56.77 MB/s
I (1166) Memory Copy: memcpy IRAM->PSRAM took 412833 CPU cycles = 56.77 MB/s
I (1216) Memory Copy: async_memcpy IRAM->PSRAM took 462112 CPU cycles = 50.72 MB/s
I (1266) Memory Copy: PIE 128-bit (16 byte loop) IRAM->PSRAM took 403204 CPU cycles = 58.13 MB/s
I (1316) Memory Copy: PIE 128-bit (32 byte loop) IRAM->PSRAM took 403204 CPU cycles = 58.13 MB/s
I (1366) Memory Copy: DSP AES3 IRAM->PSRAM took 405797 CPU cycles = 57.76 MB/s

I (1416) Memory Copy: Swapping source and destination buffers
I (1466) Memory Copy: 8-bit for loop copy PSRAM->IRAM took 1036885 CPU cycles = 22.60 MB/s
I (1516) Memory Copy: 16-bit for loop copy PSRAM->IRAM took 621030 CPU cycles = 37.74 MB/s
I (1566) Memory Copy: 32-bit for loop copy PSRAM->IRAM took 602483 CPU cycles = 38.90 MB/s
I (1616) Memory Copy: 64-bit for loop copy PSRAM->IRAM took 602412 CPU cycles = 38.91 MB/s
I (1666) Memory Copy: memcpy PSRAM->IRAM took 605499 CPU cycles = 38.71 MB/s
I (1716) Memory Copy: async_memcpy PSRAM->IRAM took 444568 CPU cycles = 52.72 MB/s
I (1766) Memory Copy: PIE 128-bit (16 byte loop) PSRAM->IRAM took 605264 CPU cycles = 38.72 MB/s
I (1816) Memory Copy: PIE 128-bit (32 byte loop) PSRAM->IRAM took 605267 CPU cycles = 38.72 MB/s
I (1866) Memory Copy: DSP AES3 PSRAM->IRAM took 608657 CPU cycles = 38.51 MB/s

I (1916) Memory Copy: Freeing 100kb from IRAM
I (1916) Memory Copy: Allocating 100kb in PSRAM, alignment: 32 bytes
I (1976) Memory Copy: 8-bit for loop copy PSRAM->PSRAM took 1412416 CPU cycles = 16.59 MB/s
I (2036) Memory Copy: 16-bit for loop copy PSRAM->PSRAM took 1051494 CPU cycles = 22.29 MB/s
I (2096) Memory Copy: 32-bit for loop copy PSRAM->PSRAM took 1045232 CPU cycles = 22.42 MB/s
I (2156) Memory Copy: 64-bit for loop copy PSRAM->PSRAM took 1045158 CPU cycles = 22.42 MB/s
I (2216) Memory Copy: memcpy PSRAM->PSRAM took 1045176 CPU cycles = 22.42 MB/s
I (2276) Memory Copy: async_memcpy PSRAM->PSRAM took 885376 CPU cycles = 26.47 MB/s
I (2336) Memory Copy: PIE 128-bit (16 byte loop) PSRAM->PSRAM took 1054630 CPU cycles = 22.22 MB/s
I (2396) Memory Copy: PIE 128-bit (32 byte loop) PSRAM->PSRAM took 1053005 CPU cycles = 22.26 MB/s
I (2456) Memory Copy: DSP AES3 PSRAM->PSRAM took 1055912 CPU cycles = 22.20 MB/s
I (2506) main_task: Returned from app_main()
```

Thanks to Microcontroller and X-Ryl669 for the hard stuff!
+ [Original Thread](https://www.esp32.com/viewtopic.php?f=13&t=36808&sid=9af3e432eb4843d96437bc07485415c8)
+ [Suggestions](https://github.com/project-x51/esp32-s3-memorycopy/issues/2)


# Version Tracking
## Version 1.1
Added IRAM_ATTR to time sensitive code blocks so they are in IRAM avoiding need to load them into instruction cache.

## Version 1.0
Original output. Known as the baseline.

