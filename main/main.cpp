/*
* Attempts to use the async_memcpy driver to copy data from one buffer to another
*
* Works for MALLOC_CAP_DMA but not if we try to align the buffers to 32 bits
* Does not work for MALLOC_CAP_SPIRAM no matter what I try.
* Reporting "invalid argument" on the call to esp_async_memcpy
*
* Using an ESP32-S3-WROOM-1U-N8R8 module
* Works with ESP-IDF 5.3.x
*  
*/


#include <inttypes.h>
#include <string>
#include <string.h>
#include <cstring>
#include <utility>
#include <type_traits>

#include "malloc.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_async_memcpy.h"
#include "esp_random.h"
#include "esp_clk_tree.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "hal/cache_hal.h"
#include "dsps_mem.h"

#include "esp_cache.h"
#include "hal/mmu_hal.h"
#include "rom/cache.h"

using namespace std;

static const char *TAG = "Memory Copy";


// Uncomment to use the PSRAM cache
#define USE_CACHE 


#define INL __attribute__((always_inline))

/**
 * @brief Uses Xtensa's zero-overhead loop to execute a given operation a number of times.
 * This function does \e not save/restore the LOOP registers, so if required these need to be 
 * saved&restored explicitly around the function call.
 * @note This may fail to assemble when compiled with \c -Og or less
 * 
 * @tparam F Type of the functor to execute
 * @tparam Args Argument types of the functor
 * @param cnt Number of iterations
 * @param f The functor to invoke
 * @param args Arguments to pass to the functor
 */
template<typename F, typename...Args>
static IRAM_ATTR inline void INL rpt(const uint32_t cnt, const F& f, Args&&...args) {

    bgn:
        asm goto (
            "LOOPNEZ %[cnt], %l[end]"
            : /* no output*/
            : [cnt] "r" (cnt)
            : /* no clobbers */
            : end
        );

            f(std::forward<Args>(args)...);

 
    end:
        /* Tell the compiler that the above code might execute more than once.
           The begin label must be before the inital LOOP asm because otherwise
           gcc may decide to put one-off setup code between the LOOP asm and the
           begin label, i.e. inside the loop.
        */
        asm goto ("":::: bgn);    
        ;
}

/*
    q<R> = *(src & ~0xf);
    src += INC;
*/
template<uint8_t R, int16_t INC = 16, typename S>
requires ( R <= 7 && ((INC & 0xf) == 0) && (-2048 <= INC) && (INC <= 2032) )
static IRAM_ATTR inline void INL vld_128_ip(S*& src) {
    asm volatile (
        "EE.VLD.128.IP q%[reg], %[src], %[inc]"
        : [src] "+r" (src)
        : [reg] "i" (R),
          [inc] "i" (INC),
          "m" (*(const uint8_t(*)[16])src)
        : 
    );
}

/*
    *(dest & ~0xf) = q<R>;
    dest += INC;
*/
template<uint8_t R, int16_t INC = 16, typename D>
requires ( R <= 7 && ((INC & 0xf) == 0) && (-2048 <= INC) && (INC <= 2032) && !std::is_const_v<D> )
static IRAM_ATTR inline void INL vst_128_ip(D*& dest) {
    asm volatile (
        "EE.VST.128.IP q%[reg], %[dest], %[inc]"
        : [dest] "+r" (dest),
          "=m" (*(uint8_t(*)[16])dest)
        : [reg] "i" (R),
          [inc] "i" (INC)
        : 
    );
}

namespace internal {

    static uint16_t cacheLineSize;

    // Cache control directly via functions provided in ROM.
    // Don't try this at home! Always use the public APIs prescribed by Espressif.

    static void fetchCacheLineSize() {
        struct cache_mode cm;
        cm.icache = 0; // data cache
        Cache_Get_Mode(&cm);
        cacheLineSize = cm.cache_line_size;
    }

    static uint32_t getCacheLineSize() {
        if(cacheLineSize == 0) [[unlikely]] {
            fetchCacheLineSize();
        }
        return cacheLineSize;
    }

    template<auto OP>
    static void onRange(void* addr, size_t size) {
        const uint32_t ls = getCacheLineSize();
        const uint32_t off = ((uint32_t)addr) & (ls-1);
        const uint32_t start = ((uint32_t)addr - off);        
        const uint32_t lines = (size + off + (ls-1)) / ls;
        OP(start,lines);
    }

    static void writeBack(void* addr, size_t size) {
        onRange<Cache_WriteBack_Items>(addr,size);
    }

    static void invalidate(void* addr, size_t size) {
        onRange<Cache_Invalidate_DCache_Items>(addr,size);
    }

    static void invalidateCache() {
        Cache_Invalidate_DCache_All();
    }

    static void clean(void* addr, size_t size) {
        onRange<Cache_Clean_Items>(addr,size);
    }

    static void cleanCache() {
        Cache_Clean_All();
    }
}

/**
 * @brief Flush and invalidate a memory region from the cache.
 * 
 * @param addr 
 * @param size 
 * @return true 
 * @return false 
 */
static inline bool flushCache(void* const addr, const size_t size) {
    return esp_cache_msync(addr,size,ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA) == ESP_OK;
    // internal::writeBack(addr,size);
    // return true;
}

/**
 * @brief Invalidate a memory region from the cache.
 * 
 * @param addr 
 * @param size 
 * @return true 
 * @return false 
 */
static inline bool invalidateCache(void* const addr, const size_t size) {
    return esp_cache_msync(addr,size,ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_TYPE_DATA) == ESP_OK;
    // internal::invalidate(addr,size);
    // return true;
}

static inline void invalidateCache() {
    internal::invalidateCache();
}

/**
 * @brief Remove data to be overwritte from the cache.
 * 
 * @param addr 
 * @param size 
 * @return true 
 * @return false 
 */
static inline bool uncacheForWrite(void* const addr, const size_t size) {
    return invalidateCache(addr,size);
    // internal::clean(addr,size);
    return true;
}

static inline bool uncacheForWrite() {
    internal::cleanCache();
    return true;
}

/**
 * @brief Remove data to be read from the cache.
 * 
 * @param addr 
 * @param size 
 * @return true 
 * @return false 
 */
static inline bool uncacheForRead(void* const addr, const size_t size) {
    return flushCache(addr,size) && invalidateCache(addr,size);
}

static inline bool isExtMem(void* const addr) {
    return mmu_hal_check_valid_ext_vaddr_region(0, (uint32_t)addr, 1, MMU_VADDR_DATA );
}


static inline void INL compiler_mem_barrier(void* const addr, const size_t size) {
    /* Let the compiler know that
        a) we need all data actually written to memory at addr before this point, and
        b) it cannot make any assumptions about the memory content at addr after this point.
    */
    asm volatile("":"+m" (*(uint8_t(*)[size])addr));
}


/**
 * @brief Preps the cache for both \p dest and \p src by flushing & invalidating any cached data.
 * 
 * @param dest destination where data will be subsequently written to
 * @param src source where data will be subsequently read from
 * @param size size in bytes of the \p dest and \p src memory regions
 * @param useCache whether to use the cache. If \c false, this function does nothing
 * @return true \p dest \e is cached memory and was successfully invalidated from the cache
 * @return false \p dest is \e not cached memory
 */
static inline bool prepareCache(void* const dest, void* const src, const size_t size, const bool useCache) 
{

    compiler_mem_barrier(src,size);

    // Do nothing if we're not using the cache
    if (! useCache)
        return false;

    if(isExtMem(src)) {
        // ESP_LOGI(TAG, "SRC is ext.");
        uncacheForRead(src,size);
    }
    if(isExtMem(dest)) {
        // ESP_LOGI(TAG, "DEST is ext.");
        return uncacheForWrite(dest,size);
    } else {
        return false;
    }
}



/// @brief The source buffer
void* _source;

/// @brief The destination buffer
void* _dest;


// Function prototypes
esp_err_t CopyBuffer(void* dest, void* source, uint32_t size, uint32_t align, bool useCache, const char *desc);
void Initialize_Buffer(void *buffer, uint32_t size);


/// @brief Async Memory copy callback implementation, running in ISR context
/// @param mcp_hdl Handle of async memcpy
/// @param event Event object, which contains related data, reserved for future
/// @param cb_args User defined arguments, passed from esp_async_memcpy function
/// @return Whether a high priority task is woken up by the callback function
static IRAM_ATTR bool dmacpy_cb(async_memcpy_t hdl, async_memcpy_event_t*, void* args) {
    const uint32_t now = esp_cpu_get_cycle_count();
    BaseType_t r = pdFALSE;
    xTaskNotifyFromISR((TaskHandle_t)args,now,eSetValueWithOverwrite,&r);
    return (r!=pdFALSE);
}


/// @brief Initializes the buffer with a pattern of data starting with a random number
/// @param buffer The buffer to initialize
/// @param size The size of the buffer in bytes
void Initialize_Buffer(void *buffer, uint32_t size)
{
    uint32_t r = esp_random();
    for (uint32_t i = 0; i < size; i++)
    {
        ((uint8_t *)buffer)[i] = i + r;
    }
}


/// @brief Calculates the bandwidth of a memory copy operation
/// @param tstart time in CPU clock cycles when the copy started
/// @param tstop time in CPU clock cycles when the copy finished
/// @param size size of the memory copies in bytes
string Calc_Bandwidth(uint32_t tstart, uint32_t tstop, uint32_t size)
{

    uint32_t f_cpu = 240000000;
    // Calculate the bandwidth in MB/s based on the current CPU clock frequency.
    if(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU,ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &f_cpu) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get CPU clock. Assuming %" PRIu32 " MHz.", f_cpu / 1000000);
    }
    uint32_t cycles = tstop - tstart;
    float seconds = (float)cycles / f_cpu; // 240000000.0f;
    float bandwidth = (float)size / (1024.0f * 1024.0f * seconds);

    // Return the bandwidth as a string
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%.2f MB/s", bandwidth);
    return string(buffer);

}


/// @brief Calculates the bandwidth of a memory copy operation
/// @param prefix The information to go before the performance infomation
/// @param tstart time in CPU clock cycles when the copy started
/// @param tstop time in CPU clock cycles when the copy finished
/// @param size size of the memory copies in bytes
void Display_Performance(string prefix, string desc, uint32_t tstart, uint32_t tstop, uint32_t size)
{
    ESP_LOGI(TAG, "%s%s took %" PRIu32 " CPU cycles = %s", 
                  prefix.c_str(), 
                  desc.c_str(),
                  tstop - tstart,
                  Calc_Bandwidth(tstart, tstop, size).c_str());
}


/// @brief Calculates the bandwidth of a memory copy operation
/// @param prefix The information to go before the performance infomation
/// @param tstart time in CPU clock cycles when the copy started
/// @param tstop time in CPU clock cycles when the copy finished
/// @param dest pointer to the buffer we copied data into
/// @param source pointer to the buffer we copied data from
/// @param size size of the memory copies in bytes
void Display_Results(string prefix, string desc, uint32_t tstart, uint32_t tstop, 
                     void* dest, void* source, uint32_t size)
{

    // Compare the destination and source buffers
    const bool match = memcmp(source,dest,size) == 0;

    // Display the performance if they match, or and error if they dont
    if (match)
        Display_Performance(prefix, desc, tstart, tstop, size);
    else
        ESP_LOGE(TAG, "%s%s failed because the buffers don't match!", prefix.c_str(), desc.c_str());

    // Give the log output some time to finish before the next test is run.
    vTaskDelay(50/portTICK_PERIOD_MS);
}


/// @brief Copies a buffer using a for loop with 8-bit pointers
/// @param T type of data to copy
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param prefix used in the debug messages to describe the type of the data copied
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
template<typename T>
static IRAM_ATTR inline void CopyBuffer_ForLoop(void* dest, void* source, uint32_t size, string prefix, const char *desc, const bool useCache)
{
    
    // Attempt the copy using code with a 32 bit pointer
    T* pSource = (T*)source;
    T* pDest = (T*)dest;
    int aCopies = size / sizeof(T);

#ifdef USE_CACHE
    // Prepare the cache
    const bool needFlush = prepareCache(dest, source, size, useCache);
#endif

    // Start our performance timer
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work - using a for loop
    // ==============================
    for (int i = 0; i < aCopies; i++) {
        pDest[i] = pSource[i];
    }

    compiler_mem_barrier(dest,size);

#ifdef USE_CACHE
    // Flush the cache if needed
    if (needFlush) {
        flushCache(dest, size);
    }
#endif

    // Display the results
    const uint32_t tstop = esp_cpu_get_cycle_count();
    Display_Results(prefix + "for loop copy ", desc, tstart, tstop, dest, source, size);
    
}


/// @brief Copies a buffer using the memcpy function
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
IRAM_ATTR esp_err_t CopyBuffer_memcpy(void* dest, void* source, uint32_t size, const char* desc, const bool useCache)
{

#ifdef USE_CACHE
    // Prepare the cache
    const bool needFlush = prepareCache(dest, source, size, useCache);
#endif

    // Start our performance timer
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work - using the memcpy function
    // =======================================
    void* ret = memcpy(dest, source, size);

#ifdef USE_CACHE
    // Flush the cache if needed
    if (needFlush) {
        // const uint32_t _s = esp_cpu_get_cycle_count();
        flushCache(dest, size);
        // const uint32_t _c = esp_cpu_get_cycle_count() - _s;
        // ESP_LOGI(TAG, "Flush took %" PRIu32 " cycles", _c);
    }
#endif

    // Display the resuilts
    const uint32_t tstop = esp_cpu_get_cycle_count();
    if (ret != 0) {
        Display_Results("memcpy ", desc, tstart, tstop, dest, source, size);
    } else {
        ESP_LOGE(TAG, "Memory copy failed");    
        return ESP_FAIL;
    }

    return ESP_OK;

}


/// @brief Copies a buffer using DMA
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
IRAM_ATTR esp_err_t CopyBuffer_DMA(void* dest, void* source, uint32_t size, uint32_t align, const char *desc)
{

    // DMA doesn't use the cache, so no cache prep needed.
    // const bool needFlush = prepareCache(dest,source,size);

    // Install the Async memcpy driver.
    ESP_LOGD(TAG, "Installing async_memcpy driver to support %" PRIu32 " byte transfers", size);
    const uint32_t PSRAM_ALIGN = align;
    const uint32_t INTRAM_ALIGN = PSRAM_ALIGN;
    //const uint32_t PSRAM_ALIGN = cache_hal_get_cache_line_size(CACHE_TYPE_DATA);
    //const uint32_t INTRAM_ALIGN = PSRAM_ALIGN;
    const async_memcpy_config_t cfg = {
        .backlog = (size+4091)/4092,
        .sram_trans_align = INTRAM_ALIGN,
        .psram_trans_align = PSRAM_ALIGN,
        .flags = 0
    };
    async_memcpy_t handle;
    esp_err_t r = esp_async_memcpy_install(&cfg, &handle);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install DMA driver: %i",r);
        return ESP_FAIL;
    }

    // Initiate a DMA copy
    ESP_LOGD(TAG, "Starting DMA copy.");
    TaskHandle_t task = xTaskGetCurrentTaskHandle();    
    const uint32_t tstart = esp_cpu_get_cycle_count();
    r = esp_async_memcpy(handle, _dest, _source, size, &dmacpy_cb, (void*)task);
    if(r == ESP_OK) {
        uint32_t tstop; // We get the tstop value from the callback via the notification.
        if(xTaskNotifyWait(0,0,&tstop,1000/portTICK_PERIOD_MS)) {
            // Display the results
            Display_Results("async_memcpy ", desc, tstart, tstop, dest, source, size);
        } else {
            ESP_LOGE(TAG, "Timed out waiting for async_memcpy.");
        }
    } else {
        ESP_LOGE(TAG, "Failed to start async_memcpy: %i",r);
    }

    // Uninstall the driver 
    esp_async_memcpy_uninstall(handle);

    return r;

}


/// @brief Copies a buffer using the ESP32-S3 PIE 128-bit memory copy instructions with 16 bytes per iteration
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
IRAM_ATTR esp_err_t CopyBuffer_PIE_128bit_16bytes(void* dest, void* source, uint32_t size, const char *desc, const bool useCache)
{
    // Copy the source to dest using the ESP32-S3 PIE 128-bit memory copy instructions

    // Setup the variables
    const uint32_t bytes_per_iteration = 16;
    const uint32_t cnt = size / bytes_per_iteration;
    const void* src_p = source;
    void* dest_p = dest;

#ifdef USE_CACHE
    // Prepare the cache
    const bool needFlush = prepareCache(dest, source, size, useCache);
#endif

    // Start our performance timer
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work - using the ESP32-S3 PIE 128-bit load/store instructions moving 16 bytes per iteration

    // Due to the extra latency of the load instruction, this code copies 16 bytes in (2+1) CPU clock cycles.
    rpt(cnt, [&src_p,&dest_p]() {
        vld_128_ip<0>(src_p); // Load 16 bytes from RAM into q0, increment src_p
        vst_128_ip<0>(dest_p); // Store 16 bytes from q0 to RAM, increment dest_p
    });

    // Same as above:
    // asm volatile (
    //     "LOOPNEZ %[cnt], end_loop%=" "\n"
    //     "   EE.VLD.128.IP q0, %[src_p], 16" "\n"
    //     "   EE.VST.128.IP q0, %[dest_p], 16" "\n"
    //     "end_loop%=:"
    //     : [src_p] "+&r" (src_p), [dest_p] "+&r" (dest_p)
    //     : [cnt] "r" (cnt)
    //     : "memory"
    // );

#ifdef USE_CACHE
    // Flush the cache if needed
    if (needFlush) {
        compiler_mem_barrier(dest,size);
        flushCache(dest, size);
    }
#endif

    // Display the resuilts
    const uint32_t tstop = esp_cpu_get_cycle_count();
    Display_Results("PIE 128-bit (16 byte loop) ", desc, tstart, tstop, dest, source, size);

    return ESP_OK;

}

/// @brief Copies a buffer using the ESP32-S3 PIE 128-bit load/store instructions in a pipeline-friendly 32-byte loop
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
IRAM_ATTR esp_err_t CopyBuffer_PIE_128bit_32bytes(void* dest, void* source, uint32_t size, const char *desc, const bool useCache)
{

    // Copy the source to dest using the ESP32-S3 PIE 128-bit load/store instructions


    // Do the work
    const uint32_t bytes_per_iteration = 32;
    const uint32_t cnt = size / bytes_per_iteration;
    const void* src_p = source;
    void* dest_p = dest;

#ifdef USE_CACHE
    // Prepare the cache
    const bool needFlush = prepareCache(dest, source, size, useCache);
#endif

    // Start our performance timer
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work - using the ESP32-S3 PIE 128-bit load/store instructions moving 32 bytes per iteration
    // ====================================================================================================
    /* Alternating access between two Q registers allows the pipeline to hide the latency
       incurred from the data dependency of successive instructions.
       Interestingly, the still-present data dependency on the _address_ register does
       _not_ induce any pipeline stalls.

       Thanks to the CPU pipeline, this code copies 32 bytes in 4 CPU clock cycles.
    */
    rpt(cnt, [&src_p,&dest_p]() {
        vld_128_ip<0>(src_p); // Load 16 bytes from RAM into q0, increment src_p
        vld_128_ip<1>(src_p); // Load 16 bytes from RAM into q1, increment src_p
        vst_128_ip<0>(dest_p); // Store 16 bytes from q0 to RAM, increment dest_p
        vst_128_ip<1>(dest_p); // Store 16 bytes from q1 to RAM, increment dest_p
    });

    // Same as above:
    // asm volatile (
    //     "LOOPNEZ %[cnt], end_loop%=" "\n"
    //     "   EE.VLD.128.IP q0, %[src_p], 16" "\n"
    //     "   EE.VLD.128.IP q1, %[src_p], 16" "\n"

    //     "   EE.VST.128.IP q0, %[dest_p], 16" "\n"
    //     "   EE.VST.128.IP q1, %[dest_p], 16" "\n"
    //     "end_loop%=:"
    //     : [src_p] "+&r" (src_p), [dest_p] "+&r" (dest_p)
    //     : [cnt] "r" (cnt)
    //     : "memory"
    // );

#ifdef USE_CACHE
    // Flush the cache if needed
    if (needFlush) {
        compiler_mem_barrier(dest,size);
        flushCache(dest, size);
    }
#endif

    // Display the resuilts
    const uint32_t tstop = esp_cpu_get_cycle_count();
    Display_Results("PIE 128-bit (32 byte loop) ", desc, tstart, tstop, dest, source, size);

    return ESP_OK;

}


/// @brief Copies a buffer using the ESP32-S3 dsp memory copy instructions
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
IRAM_ATTR esp_err_t CopyBuffer_DSP(void* dest, void* source, uint32_t size, const char *desc, const bool useCache)
{

#ifdef USE_CACHE
    // Prepare the cache
    const bool needFlush = prepareCache(dest, source, size, useCache);
#endif

    // Start our performance timer
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work - using the ESP32-S3 dsp memory copy instructions
    void *ret = dsps_memcpy_aes3(dest, source, size);
    if (!ret) {
        ESP_LOGE(TAG, "Failed to execute dsps_memcpy_aes3.");
        return ESP_FAIL;
    }

#ifdef USE_CACHE
    // Flush the cache if needed
    if (needFlush) {
        compiler_mem_barrier(dest,size);
        flushCache(dest, size);
    }
#endif

    // Display the resuilts
    const uint32_t tstop = esp_cpu_get_cycle_count();
    Display_Results("DSP AES3 ", desc, tstart, tstop, dest, source, size);

    return ESP_OK;

}

/**
 * @brief Fill \p dest with 0 and make sure the data has passed the cache.
 * 
 * @param dest 
 * @param size 
 */
static void clearBuffer(void* dest, size_t size) {
    memset(dest,0,size);
    if(isExtMem(dest)) {
        flushCache(dest,size);
    }    
}

/// @brief Copies a buffer using all the different methods
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
IRAM_ATTR esp_err_t CopyBuffer(void* dest, void* source, uint32_t size, uint32_t align, bool useCache, const char *desc)
{

    // Wait a moment for previous logging to finish.
    vTaskDelay(50/portTICK_PERIOD_MS);

    clearBuffer(dest,size);

    // No meaningful difference between a for loop and a while loop
    CopyBuffer_ForLoop<uint8_t>(dest, source, size, "8-bit ", desc, useCache);
    
    clearBuffer(dest,size);

    CopyBuffer_ForLoop<uint16_t>(dest, source, size, "16-bit ", desc, useCache);

    clearBuffer(dest,size);

    CopyBuffer_ForLoop<uint32_t>(dest, source, size, "32-bit ", desc, useCache);

    clearBuffer(dest,size);

    CopyBuffer_ForLoop<uint64_t>(dest, source, size, "64-bit ", desc, useCache);

    clearBuffer(dest,size);

    // CopyBuffer_8BitForLoop(dest, source, size, desc);
    // CopyBuffer_16BitForLoop(dest, source, size, desc);
    // CopyBuffer_32BitForLoop(dest, source, size, desc);
    // CopyBuffer_64BitForLoop(dest, source, size, desc);

    CopyBuffer_memcpy(dest, source, size, desc, useCache);

    clearBuffer(dest,size);

    CopyBuffer_DMA(dest, source, size, align, desc);

    clearBuffer(dest,size);

    CopyBuffer_PIE_128bit_16bytes(dest, source, size, desc, useCache);

    clearBuffer(dest,size);

    CopyBuffer_PIE_128bit_32bytes(dest, source, size, desc, useCache);

    clearBuffer(dest,size);

    CopyBuffer_DSP(dest, source, size, desc, useCache);

    clearBuffer(dest,size);

    return ESP_OK;

}


/// @brief Copies memory between various RAM types using different methods and benchmarks the performance
/// @param size The size of the memory to copy
/// @param align The alignment size to use when allocating the memory
void IRAM_ATTR MemoryCopy_V1(uint32_t size, uint32_t align)
{

    // Decide whether to use the PSRAM cache
    bool useCache = false;
#ifdef USE_CACHE
    useCache = true;
#endif

    // Hello world
    ESP_LOGI(TAG, "\n\nmemory copy version 1.3\n");
    if (useCache)
        ESP_LOGI(TAG, "Using PSRAM CACHE flush\n");
    else
        ESP_LOGI(TAG, "NOT flushing PSRAM CACHE\n");

    // Test copying from IRAM to IRAM using 32 byte alignment
    ESP_LOGI(TAG, "Allocating 2 x %" PRIu32 "kb in IRAM, alignment: %" PRIu32 " bytes", size/1024, align);
    _source = heap_caps_aligned_alloc(align, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    _dest = heap_caps_aligned_alloc(align, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if(!_dest || !_source) {
        ESP_LOGE(TAG, "Memory Allocation failed");
        return;
    }
    Initialize_Buffer(_source, size);
    CopyBuffer(_dest, _source, size, align, false, "IRAM->IRAM");

    // Test copying from IRAM to PSRAM using 32 byte alignment
    printf("\n");
    ESP_LOGI(TAG, "Freeing %" PRIu32 "kb from IRAM", size/1024);
    free(_dest);
    ESP_LOGI(TAG, "Allocating %" PRIu32 "kb in PSRAM, alignment: %" PRIu32 " bytes", size/1024, align);
    _dest = heap_caps_aligned_alloc(align, size, MALLOC_CAP_SPIRAM);
    if(!_dest || !_source) {
        ESP_LOGE(TAG, "Memory Allocation failed");
        return;
    }
    CopyBuffer(_dest, _source, size, align, useCache, "IRAM->PSRAM");

    // Test copying from PSRAM to IRAM using 32 byte alignment
    printf("\n");
    ESP_LOGI(TAG, "Swapping source and destination buffers");
    { void* temp = _source; _source = _dest; _dest = temp; }
    Initialize_Buffer(_source,size); // Fill the new source buffer with data.
    CopyBuffer(_dest, _source, size, align, useCache, "PSRAM->IRAM");

    // Test copying from PSRAM to PSRAM using 32 byte alignment
    printf("\n");
    ESP_LOGI(TAG, "Freeing %" PRIu32 "kb from IRAM", size/1024);
    free(_dest);
    ESP_LOGI(TAG, "Allocating %" PRIu32 "kb in PSRAM, alignment: %" PRIu32 " bytes", size/1024, align);
    _dest = heap_caps_aligned_alloc(align, size, MALLOC_CAP_SPIRAM);
    if(!_dest || !_source) {
        ESP_LOGE(TAG, "Memory Allocation failed");
        return;
    }
    CopyBuffer(_dest, _source, size, align, useCache, "PSRAM->PSRAM");

    // Free the memory
    free(_source);
    free(_dest);

}


/// @brief Main application entry point
extern "C" void app_main(void)
{
    // Run the copy on 100KB of data with cache line alignment
    MemoryCopy_V1(100 * 1024, internal::getCacheLineSize());
}