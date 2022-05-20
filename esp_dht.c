/*****************************************************************************

    Copyright 2022 SZIGETI János

    This file is part of BilisEspDht component.

    BilisEspDht is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BilisEspDht is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*****************************************************************************/
#include <string.h>
#include "esp_dht.h"

/* DEFINES */

#define DHT_SYNC_BITS 2U
#define DHT_DATA_BITS (DHT_DATA_BYTES * 8U)
#define DHT_HOSTPULLDOWN_IVAL_US 1000U
#define DHT_BIT1_IVAL_LO_US 68
#define DHT_BIT1_IVAL_HI_US 75
#define DHT_BIT0_IVAL_LO_US 22
#define DHT_BIT0_IVAL_HI_US 29


/* STATIC FUNCTION PRE */

static void IRAM_ATTR dht_gpio_edge_handler(void* pvParam);
static bool IRAM_ATTR dht_host_ready_handler(void *pvParam);

/*  INTERNAL FUNCTIONS */

/// ISR on data channel signal edge.
/// \param pvParam DHT communication parameter.
static void IRAM_ATTR dht_gpio_edge_handler(void* pvParam) {
    S_DHT_COMM_CONFIG *psCfg = &((S_DHT_COMM_PARAM*) pvParam)->sCfg;
    S_DHT_COMM_VAR *psVar = &((S_DHT_COMM_PARAM*) pvParam)->sVar;
    uint64_t u64usClock;
    uint32_t *pu32usClock = (uint32_t*)&u64usClock; // we deal with 32 bit time differences
    int iLevel;

    timer_get_counter_value(psCfg->eTGroup, psCfg->eTIdx, &u64usClock);
    iLevel = gpio_get_level(psCfg->eDatPin);

    if (iLevel) {   // pull up
        psVar->u32usLastDevPullUp = *pu32usClock;
    } else {    // pull down
        if (psVar->u8BitIdx < DHT_DATA_BITS) {
            uint32_t u32Diff = *pu32usClock - psVar->u32usLastDevPullUp;
            if (DHT_BIT1_IVAL_LO_US <= u32Diff && u32Diff <= DHT_BIT1_IVAL_HI_US) { // set bit
                psVar->acDat[psVar->u8BitIdx / 8] |= 1 << (7 - (psVar->u8BitIdx % 8));
            } else if (DHT_BIT0_IVAL_LO_US <= u32Diff && u32Diff <= DHT_BIT0_IVAL_HI_US) { // clr bit
                // do nothing
            } else {    // bit read error
                psVar->b32Flag|=DHT_FLAG_BIT_READ_ERR;
            }
        } else {    // sync bits or overflow bits
            if ((uint8_t)(psVar->u8BitIdx + DHT_SYNC_BITS) < psVar->u8BitIdx) { // pre bits
                // do nothing
                // TODO check/log sync length
            } else {    // overflow
                psVar->b32Flag|=DHT_FLAG_BIT_OVERFLOW_ERR;
            }
        }
        ++psVar->u8BitIdx;  // intentionally incerased even in case of bit overflow

        if (psVar->u8BitIdx == DHT_DATA_BITS) { // ready
            psVar->b32Flag &= ~DHT_FLAG_PHASE_DEV;  // clr phase flag
            psVar->b32Flag |= DHT_FLAG_READY;   // set ready flag
            if (((psVar->acDat[0] + psVar->acDat[1] + psVar->acDat[2] + psVar->acDat[3])&0xFF) == psVar->acDat[4]) {
                // checksum matches
                psVar->b32Flag |= DHT_FLAG_VALID;
            } else {    // checksum fails
                // leave valid flag unset
            }
            if (NULL != psCfg->pfReadyCallback) {
                psCfg->pfReadyCallback(psCfg->pvReadyParam);
            }
        }
    }
}

/// ISR triggered on timer alarm (end of host sync signal).
/// Changes communication direction: out -> in.
/// \param pvParam (Pointer to) DHT communication parameter.
/// \return true.
static bool IRAM_ATTR dht_host_ready_handler(void *pvParam) {
    S_DHT_COMM_CONFIG *psCfg = &((S_DHT_COMM_PARAM*) pvParam)->sCfg;
    S_DHT_COMM_VAR *psVar = &((S_DHT_COMM_PARAM*) pvParam)->sVar;
    uint64_t u64usClock;

    timer_get_counter_value(psCfg->eTGroup, psCfg->eTIdx, &u64usClock);
    psVar->u32usHostPullUp = (uint32_t)u64usClock;
    psVar->b32Flag &= ~DHT_FLAG_PHASE_HOST;
    psVar->b32Flag |= DHT_FLAG_PHASE_DEV;
    gpio_set_level(psCfg->eDatPin, 1);
    gpio_pullup_en(psCfg->eDatPin);
    gpio_set_direction(psCfg->eDatPin, GPIO_MODE_INPUT);
    return true;
}

/* INTERFACE FUNCTIONS */

/// Reads DHT22 values (non-blocking).
/// \param psParam DHT communication parameters. sCfg must be defined, sVar will be overwritten.
/// This memory area must be available during the whole communication process.
/// \return Failure.
bool dht_read_nb(S_DHT_COMM_PARAM *psParam) {
    S_DHT_COMM_CONFIG *psCfg = &psParam->sCfg;
    S_DHT_COMM_VAR *psVar = &psParam->sVar;
    uint64_t u64usClock;

    // cleanup
    psVar->b32Flag = DHT_FLAG_PHASE_HOST;
    psVar->u8BitIdx = (uint8_t)-(DHT_SYNC_BITS); // underflow sync bits
    memset(psVar->acDat, 0, DHT_DATA_BYTES);
    gpio_isr_handler_remove(psCfg->eDatPin);

    // init dat gpio port
    gpio_pullup_en(psCfg->eDatPin);
    gpio_set_direction(psCfg->eDatPin, GPIO_MODE_OUTPUT);
    gpio_set_intr_type(psCfg->eDatPin, GPIO_INTR_ANYEDGE);
    psVar->b32Flag |=
            (ESP_OK != gpio_isr_handler_add(psCfg->eDatPin, dht_gpio_edge_handler, (void*) psParam) ?
            DHT_FLAG_REGISTER_GPIO_ISR_ERR :
            0);

    // host start signal
    timer_get_counter_value(psCfg->eTGroup, psCfg->eTIdx, &u64usClock);
    psVar->u32usHostPullDown = (uint32_t)u64usClock;
    gpio_set_level(psCfg->eDatPin, 0);
    timer_set_alarm_value(psCfg->eTGroup, psCfg->eTIdx, u64usClock + DHT_HOSTPULLDOWN_IVAL_US);
    psVar->b32Flag |=
            (ESP_OK != timer_isr_callback_add(
            psCfg->eTGroup, psCfg->eTIdx, dht_host_ready_handler, (void*) psParam, 0) ?
            DHT_FLAG_REGISTER_TIMER_ISR_ERR :
            0);
    timer_enable_intr(psCfg->eTGroup, psCfg->eTIdx);
    timer_set_alarm(psCfg->eTGroup, psCfg->eTIdx, TIMER_ALARM_EN);

    return 0 != (psVar->b32Flag & DHT_FLAG_ANYERR);
}
