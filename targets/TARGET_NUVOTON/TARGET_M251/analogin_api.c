/*
 * Copyright (c) 2019, Nuvoton Technology Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "analogin_api.h"

#if DEVICE_ANALOGIN

#include "cmsis.h"
#include "pinmap.h"
#include "PeripheralPins.h"
#include "gpio_api.h"
#include "nu_modutil.h"
#include "hal/PinNameAliases.h"

static uint32_t eadc_modinit_mask = 0;

static const struct nu_modinit_s adc_modinit_tab[] = {
    {ADC_0_0, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_1, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_2, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_3, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_4, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_5, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_6, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_7, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_8, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_9, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_10, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_11, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_12, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_13, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_14, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
    {ADC_0_15, EADC_MODULE, 0, CLK_CLKDIV0_EADC(8), EADC_RST, EADC_INT0_IRQn, NULL},
};

#if defined(MBED_CONF_TARGET_EADC_EXTSMPT_LIST)
/* Structure for extending sampling time on per-pin basis */
struct nu_eadc_extsmpt {
    PinName     pin;
    uint32_t    value;
};

static struct nu_eadc_extsmpt eadc_extsmpt_arr[] = {
    MBED_CONF_TARGET_EADC_EXTSMPT_LIST
};
#endif

void analogin_init(analogin_t *obj, PinName pin)
{
    obj->adc = (ADCName) pinmap_peripheral(pin, PinMap_ADC);
    MBED_ASSERT(obj->adc != (ADCName) NC);

    const struct nu_modinit_s *modinit = get_modinit(obj->adc, adc_modinit_tab);
    MBED_ASSERT(modinit != NULL);
    MBED_ASSERT(modinit->modname == (int) obj->adc);

    obj->pin = pin;

    // Wire pinout
    pinmap_pinout(pin, PinMap_ADC);

    EADC_T *eadc_base = (EADC_T *) NU_MODBASE(obj->adc);

    // NOTE: All channels (identified by ADCName) share a ADC module. This reset will also affect other channels of the same ADC module.
    if (! eadc_modinit_mask) {
        // Select clock source
        CLK_SetModuleClock(modinit->clkidx, modinit->clksrc, modinit->clkdiv);

        // Enable clock
        CLK_EnableModuleClock(modinit->clkidx);

        // Reset this module if no channel enabled
        SYS_ResetModule(modinit->rsetidx);

        /* Configure EADC_module to be ready to convert
         * M251 doesn't support 'input mode' feature. Configure it to 0. */
        EADC_Open(eadc_base, 0);
    }

    uint32_t chn =  NU_MODSUBINDEX(obj->adc);

    // Configure the sample module Nmod for analog input channel Nch and software trigger source
    EADC_ConfigSampleModule(eadc_base, chn, EADC_SOFTWARE_TRIGGER, chn);

    /* Extend sampling time (in EADC clocks)
     *
     * Some targets like NUMAKER_M252KG may fail to pass CI Test Shield test
     * without extending sampling time.
     */
    EADC_SetExtendSampleTime(eadc_base, chn, 10);

#if defined(MBED_CONF_TARGET_EADC_EXTSMPT_LIST)
    // Extend sampling time in EADC clocks on per-pin basis
    struct nu_eadc_extsmpt *eadc_extsmpt_pos = eadc_extsmpt_arr;
    struct nu_eadc_extsmpt *eadc_extsmpt_end = eadc_extsmpt_arr + sizeof (eadc_extsmpt_arr) / sizeof (eadc_extsmpt_arr[0]);
    for (; eadc_extsmpt_pos != eadc_extsmpt_end; eadc_extsmpt_pos ++) {
        if (eadc_extsmpt_pos->pin == pin) {
            EADC_SetExtendSampleTime(eadc_base, chn, eadc_extsmpt_pos->value);
            break;
        }
    }
#endif

    eadc_modinit_mask |= 1 << chn;
}

void analogin_free(analogin_t *obj)
{
    const struct nu_modinit_s *modinit = get_modinit(obj->adc, adc_modinit_tab);
    MBED_ASSERT(modinit->modname == (int) obj->adc);

    /* Module subindex (aka channel) */
    uint32_t chn =  NU_MODSUBINDEX(obj->adc);

    EADC_T *eadc_base = (EADC_T *) NU_MODBASE(obj->adc);

    /* Channel-level windup from here */

    /* Mark channel free */
    eadc_modinit_mask &= ~(1 << chn);

    /* Module-level windup from here */

    /* See analogin_init() for reason */
    if (! eadc_modinit_mask) {
        /* Disable EADC module */
        EADC_Close(eadc_base);

        /* Disable IP clock */
        CLK_DisableModuleClock(modinit->clkidx);
    }
    
    /* Free up pins */
    gpio_set(obj->pin);
    obj->pin = NC;
}

uint16_t analogin_read_u16(analogin_t *obj)
{
    EADC_T *eadc_base = (EADC_T *) NU_MODBASE(obj->adc);
    uint32_t chn =  NU_MODSUBINDEX(obj->adc);

    EADC_START_CONV(eadc_base, 1 << chn);
    while (EADC_GET_DATA_VALID_FLAG(eadc_base, 1 << chn) != ((uint32_t) (1 << chn)));
    uint16_t conv_res_12 = EADC_GET_CONV_DATA(eadc_base, chn);
    // Just 12 bits are effective. Convert to 16 bits.
    // conv_res_12: 0000 b11b10b9b8 b7b6b5b4 b3b2b1b0
    // conv_res_16: b11b10b9b8 b7b6b5b4 b3b2b1b0 b11b10b9b8
    uint16_t conv_res_16 = (conv_res_12 << 4) | (conv_res_12 >> 8);

    return conv_res_16;
}

float analogin_read(analogin_t *obj)
{
    uint16_t value = analogin_read_u16(obj);
    return (float) value * (1.0f / (float) 0xFFFF);
}

const PinMap *analogin_pinmap()
{
    return PinMap_ADC;
}

#endif
