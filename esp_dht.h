#ifndef ESP_DHT_H
#define ESP_DHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "driver/timer.h"

#define DHT_DATA_BYTES 5U

#define DHT_FLAG_READY 1U
#define DHT_FLAG_VALID 2U
#define DHT_FLAG_PHASE_HOST 4U
#define DHT_FLAG_PHASE_DEV 8U
#define DHT_FLAG_REGISTER_GPIO_ISR_ERR 16U
#define DHT_FLAG_REGISTER_TIMER_ISR_ERR 32U
#define DHT_FLAG_BIT_READ_ERR 64U
#define DHT_FLAG_BIT_OVERFLOW_ERR 128U


typedef struct {
    uint32_t u32usHostPullDown;
    uint32_t u32usHostPullUp;
    uint32_t u32usLastDevPullUp;
    uint8_t u8BitIdx;
    uint32_t b32Flag;
    char acDat[DHT_DATA_BYTES];
} S_DHT_COMM_VAR;

typedef struct {
    gpio_num_t eDatPin;
    timer_group_t eTGroup;
    timer_idx_t eTIdx;
    void (*pfReadyCallback)(void*);
    void *pvReadyParam;
} S_DHT_COMM_CONFIG;

typedef struct {
    const S_DHT_COMM_CONFIG sCfg;
    S_DHT_COMM_VAR sVar;
} S_DHT_COMM_PARAM;


void dht_read_nb(S_DHT_COMM_PARAM *psParam);


#ifdef __cplusplus
}
#endif

#endif /* ESP_DHT_H */

