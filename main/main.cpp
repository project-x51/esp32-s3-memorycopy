/*
* Attempts to use the async_memcpy driver to copy data from one buffer to another
*
* Works for MALLOC_CAP_DMA but not if we try to align the buffers to 32 bits
* Does not work for MALLOC_CAP_SPIRAM no matter what I try.
* Reporting "invalid argument" on the call to esp_async_memcpy
*
* Using an ESP32-S3-WROOM-1U-N8R8 module
* Works with ESP-IDF 5.1.1
*  
*/


#include <inttypes.h>
#include <string>
#include <string.h>

#include "malloc.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_async_memcpy.h"
#include "esp_random.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "hal/cache_hal.h"
#include "dsps_mem.h"

using namespace std;

static const char *TAG = "Memory Copy";


/**
 * @brief Uses Xtensa's zero-overhead loop to execute a given operation a number of times.
 * This function does \e not save/restore the LOOP registers, so if required these need to be 
 * saved&restored explicitly around the function call.
 * 
 * @tparam F Type of the functor to execute
 * @tparam Args Argument types of the functor
 * @param cnt Number of iterations
 * @param f The functor to invoke
 * @param args Arguments to pass to the functor
 */
// template<typename F, typename...Args>
// static inline void INT rpt(const uint32_t cnt, const F& f, Args&&...args) {

//     bgn:
//         asm goto (
//             "LOOPNEZ %[cnt], %l[end]"
//             : /* no output*/
//             : [cnt] "r" (cnt)
//             : /* no clobbers */
//             : end
//         );

//             f(std::forward<Args>(args)...);

 
//     end:
//         /* Tell the compiler that the above code might execute more than once.
//            The begin label must be before the inital LOOP asm because otherwise
//            gcc may decide to put one-off setup code between the LOOP asm and the
//            begin label, i.e. inside the loop.
//         */
//         asm goto ("":::: bgn);    
//         ;
// }


/// @brief The source buffer
void* _source;

/// @brief The destination buffer
void* _dest;


// Function prototypes
esp_err_t CopyBuffer(void* dest, void* source, uint32_t size, uint32_t align, char *desc);
void Initialize_Buffer(void *buffer, uint32_t size);



/// @brief Copies memory between various RAM types using different methods and benchmarks the performance
/// @param size The size of the memory to copy
/// @param align The alignment size to use when allocating the memory
void MemoryCopy_V1(uint32_t size, uint32_t align)
{

    // Hello world
    ESP_LOGI(TAG, "\n\nmemory copy version 1.\n");

    // Test copying from IRAM to IRAM using 32 byte alignment
    ESP_LOGI(TAG, "Allocating 2 x %" PRIu32 "kb in IRAM, alignment: %" PRIu32 " bytes", size/1024, align);
    _source = heap_caps_aligned_alloc(align, size, MALLOC_CAP_DMA);
    _dest = heap_caps_aligned_alloc(align, size, MALLOC_CAP_DMA);
    if(!_dest || !_source) {
        ESP_LOGE(TAG, "Memory Allocation failed");
        return;
    }
    Initialize_Buffer(_source, size);
    CopyBuffer(_source, _dest, size, align, "IRAM->IRAM");

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
    CopyBuffer(_source, _dest, size, align, "IRAM->PSRAM");

    // Test copying from PSRAM to IRAM using 32 byte alignment
    printf("\n");
    ESP_LOGI(TAG, "Swapping source and destination buffers");
    void* temp = _source; _source = _dest; _dest = temp;
    CopyBuffer(_source, _dest, size, align, "PSRAM->IRAM");

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
    CopyBuffer(_source, _dest, size, align, "PSRAM->PSRAM");

    // Free the memory
    free(_source);
    free(_dest);

}


/// @brief Async Memory copy callback implementation, running in ISR context
/// @param mcp_hdl Handle of async memcpy
/// @param event Event object, which contains related data, reserved for future
/// @param cb_args User defined arguments, passed from esp_async_memcpy function
/// @return Whether a high priority task is woken up by the callback function
static bool dmacpy_cb(async_memcpy_t hdl, async_memcpy_event_t*, void* args) {
    BaseType_t r = pdFALSE;
    xTaskNotifyFromISR((TaskHandle_t)args,0,eNoAction,&r);
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

    // Calculate the bandwidth in MB/s given a clock frequency of 240Mhz
    uint32_t cycles = tstop - tstart;
    float seconds = (float)cycles / 240000000.0f;
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
    bool match = true;
    for (uint32_t i = 0; i < size; i++) {
        if (((uint8_t*)source)[i] != ((uint8_t*)dest)[i]) {
            match = false;
            break;  
        }
    }

    // Display the performance if they match, or and error if they dont
    if (match)
        Display_Performance(prefix, desc, tstart, tstop, size);
    else
        ESP_LOGE(TAG, "%s%s failed because the buffers don't match!", prefix.c_str(), desc.c_str());

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
static inline void CopyBuffer_ForLoop(void* dest, void* source, uint32_t size, string prefix, char *desc)
{
    
    // Attempt the copy using code with a 32 bit pointer
    T* pSource = (T*)source;
    T* pDest = (T*)dest;
    int aCopies = size / sizeof(T);
    uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work
    for (int i = 0; i < aCopies; i++) {
        pDest[i] = pSource[i];
    }

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
esp_err_t CopyBuffer_memcpy(void* dest, void* source, uint32_t size, char *desc)
{

    // Copy the source to dest using the memcpy function
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work
    void* ret = memcpy(dest, source, size);

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
esp_err_t CopyBuffer_DMA(void* dest, void* source, uint32_t size, uint32_t align, char *desc)
{

    // Install the Async memcpy driver.
    ESP_LOGD(TAG, "Installing async_memcpy driver to support %" PRIu32 " byte transfers", size);
    const uint32_t PSRAM_ALIGN = align;
    const uint32_t INTRAM_ALIGN = PSRAM_ALIGN;
    //const uint32_t PSRAM_ALIGN = cache_hal_get_cache_line_size(CACHE_TYPE_DATA);
    //const uint32_t INTRAM_ALIGN = PSRAM_ALIGN;
    const async_memcpy_config_t cfg = {
        .backlog = (size+4091)/4065,
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
    esp_err_t ret;
    ESP_LOGD(TAG, "Starting DMA copy.");
    const uint32_t tstart = esp_cpu_get_cycle_count();
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    r = esp_async_memcpy(handle, _dest, _source, size, &dmacpy_cb, (void*)task);
    if(r == ESP_OK) {
        if(xTaskNotifyWait(0,0,0,1000/portTICK_PERIOD_MS)) {

            // Display the results
            const uint32_t tstop = esp_cpu_get_cycle_count();
            Display_Results("async_memcpy ", desc, tstart, tstop, dest, source, size);
            ret = ESP_OK;
        } else {
            ESP_LOGE(TAG, "Timed out waiting for async_memcpy.");
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Failed to start async_memcpy: %i",r);
        ret = ESP_FAIL;
    }

    // Uninstall the driver 
    esp_async_memcpy_uninstall(handle);

    return ESP_OK;

}


/// @brief Copies a buffer using the ESP32-S3 PIE 128-bit memory copy instructions
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
esp_err_t CopyBuffer_PIE_128bit(void* dest, void* source, uint32_t size, char *desc)
{

    // Copy the source to dest using the ESP32-S3 PIE 128-bit memory copy instructions
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work
    // const uint8_t* src_p = (uint8_t*)source;
    // uint8_t* dest_p = (uint8_t*)dest;
    // rpt(size / 32, [&src_p, &dest_p]() {
    //     src_p >> Q0;    // a.k.a. simd::ops::vld_128_ip<0,16>(src);
    //     src_p >> Q1;
    //     dest_p << Q0;   // a.k.a. simd::ops::vst_128_ip<0,16>(dest);
    //     dest_p << Q1;
    //     }
    // );

    // Do the work
    const uint32_t bytes_per_iteration = 32;
    const uint32_t cnt = size / bytes_per_iteration;
    const uint8_t* src_p = (uint8_t*)source;
    uint8_t* dest_p = (uint8_t*)dest;
    asm volatile (
        "LOOPNEZ %[cnt], end_loop%=" "\n"
        "   EE.VLD.128.IP q0, %[src_p], 16" "\n"
        "   EE.VLD.128.IP q1, %[src_p], 16" "\n"

        "   EE.VST.128.IP q0, %[dest_p], 16" "\n"
        "   EE.VST.128.IP q1, %[dest_p], 16" "\n"
        "end_loop%=:"
        : [src_p] "+&r" (src_p), [dest_p] "+&r" (dest_p)
        : [cnt] "r" (cnt)
        : "memory"
    );

    // Display the resuilts
    const uint32_t tstop = esp_cpu_get_cycle_count();
    Display_Results("PIE 128-bit ", desc, tstart, tstop, dest, source, size);

    return ESP_OK;

}


/// @brief Copies a buffer using the ESP32-S3 dsp memory copy instructions
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
esp_err_t CopyBuffer_DSP(void* dest, void* source, uint32_t size, char *desc)
{

    // Copy the source to dest using the ESP32-S3 dsp memory copy instructions
    const uint32_t tstart = esp_cpu_get_cycle_count();

    // Do the work
    void *ret = dsps_memcpy_aes3(dest, source, size);
    if (!ret) {
        ESP_LOGE(TAG, "Failed to execute dsps_memcpy_aes3.");
        return ESP_FAIL;
    }
    
    // Display the resuilts
    const uint32_t tstop = esp_cpu_get_cycle_count();
    Display_Results("DSP AES3 ", desc, tstart, tstop, dest, source, size);

    return ESP_OK;

}


/// @brief Copies a buffer using all the different methods
/// @param dest pointer to the buffer to copy to
/// @param source pointer to the buffer to copy from
/// @param size amount of memory to copy
/// @param desc used in the debug messages to describe the copy
/// @return ESP_OK if successful. Otherwise ESP_FAIL
esp_err_t CopyBuffer(void* dest, void* source, uint32_t size, uint32_t align, char *desc)
{

    // No meaningful difference between a for loop and a while loop
    CopyBuffer_ForLoop<uint8_t>(dest, source, size, "8-bit ", desc);
    CopyBuffer_ForLoop<uint16_t>(dest, source, size, "16-bit ", desc);
    CopyBuffer_ForLoop<uint32_t>(dest, source, size, "32-bit ", desc);
    CopyBuffer_ForLoop<uint64_t>(dest, source, size, "64-bit ", desc);

    // CopyBuffer_8BitForLoop(dest, source, size, desc);
    // CopyBuffer_16BitForLoop(dest, source, size, desc);
    // CopyBuffer_32BitForLoop(dest, source, size, desc);
    // CopyBuffer_64BitForLoop(dest, source, size, desc);

    CopyBuffer_memcpy(dest, source, size, desc);

    CopyBuffer_DMA(dest, source, size, align, desc);

    CopyBuffer_PIE_128bit(dest, source, size, desc);

    CopyBuffer_DSP(dest, source, size, desc);

    return ESP_OK;

}




extern "C"
{
    void app_main(void);
}

/// @brief Main application entry point
void app_main(void)
{
    // Run the copy on 100KB of data with 32 byte alignment
    MemoryCopy_V1(100 * 1024, 32);
}