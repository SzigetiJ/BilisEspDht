BilisEspDht
===========

A non-blocking DHT22 sensor data reader component for ESP-IDF platform (ESP32).

Usage
-----

1. Get this component (clone or download it);

2. Integrate the component into your project (either directly or by setting `EXTRA_COMPONENT_DIRS` in *CMakeLists.txt*);

3. Set up a µs resolution timer, e.g.,
```C
static void init_timer(timer_group_t eGroup, timer_idx_t eIdx, uint32_t u32Divider) {
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = u32Divider,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_DIS,
        .auto_reload = false,
    }; // default clock source is APB
    timer_init(eGroup, eIdx, &config);
    timer_set_counter_value(eGroup, eIdx, 0);
    timer_start(eGroup, eIdx);
}
/* [...] */
    init_timer(TIMER_GROUP_1, TIMER_0, 80);
```

4. Define a read callback function, e.g.,
```C
void print_dht_result(void *pvParam) {
    char *acDat = (char *)pvParam;
    uint16_t u16Hum = (uint16_t)acDat[0]<<8|acDat[1];
    uint16_t u16Temp = (uint16_t)acDat[2]<<8|acDat[3];
    // do anything with these values
}
```

5. Call non-blocking read function with configured parameter, e.g.,
```C
    S_DHT_COMM_PARAM sDHT0Param = {.sCfg =
        {
            GPIO_NUM_19,
            TIMER_GROUP_1,
            TIMER_0,
            print_dht_result,
            NULL
        }};
    sDHT0Param.sCfg.pvReadyParam = (void*)sDHT0Param.sVar.acDat;
    bool bRes = dht_read_nb(&sDHT0Param);
```

6. Check `bRes` for errors, wait for the callback function (in out case: `print_dht_result`) or
see communication internals in `sDHT0Param.sVar`.