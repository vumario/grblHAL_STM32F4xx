/*
  btt_skr_pro.c - driver code for STM32F4xx ARM processors

  Part of grblHAL

  Copyright (c) 2020-2021 Terje Io

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "driver.h"

#if defined(BOARD_SKR_PRO_1_1)

#include <math.h>
#include <string.h>

#include "main.h"
#include "spi.h"
#include "grbl/protocol.h"
#include "grbl/settings.h"

#if TRINAMIC_ENABLE == 2130 || TRINAMIC_ENABLE == 5160

static axes_signals_t tmc;
static uint32_t n_axis;
static TMC_spi_datagram_t datagram[N_AXIS];

TMC_spi_status_t tmc_spi_read (trinamic_motor_t driver, TMC_spi_datagram_t *reg)
{
    static TMC_spi_status_t status = 0;

    uint8_t res;
    uint_fast8_t idx = N_AXIS, ridx = 0;
    uint32_t f_spi = spi_set_speed(SPI_BAUDRATEPRESCALER_32);
    volatile uint32_t dly = 100;

    datagram[driver.axis].addr.value = reg->addr.value;
    datagram[driver.axis].addr.write = 0;

    BITBAND_PERI(TRINAMIC_CS_PORT->ODR, TRINAMIC_CS_PIN) = 0;

    do {
        if(bit_istrue(tmc.mask, bit(--idx))) {
            spi_put_byte(datagram[idx].addr.value);
            spi_put_byte(0);
            spi_put_byte(0);
            spi_put_byte(0);
            spi_put_byte(0);
        }
    } while(idx);

    while(--dly);

    BITBAND_PERI(TRINAMIC_CS_PORT->ODR, TRINAMIC_CS_PIN) = 1;

    dly = 50;
    while(--dly);

    BITBAND_PERI(TRINAMIC_CS_PORT->ODR, TRINAMIC_CS_PIN) = 0;

    idx = N_AXIS;
    do {
        ridx++;
        if(bit_istrue(tmc.mask, bit(--idx))) {

            res = spi_put_byte(datagram[idx].addr.value);

            if(N_AXIS - ridx == driver.axis) {
                status = res;
                reg->payload.data[3] = spi_get_byte();
                reg->payload.data[2] = spi_get_byte();
                reg->payload.data[1] = spi_get_byte();
                reg->payload.data[0] = spi_get_byte();
            } else {
                spi_get_byte();
                spi_get_byte();
                spi_get_byte();
                spi_get_byte();
            }
        }
    } while(idx);

    dly = 100;
    while(--dly);

    BITBAND_PERI(TRINAMIC_CS_PORT->ODR, TRINAMIC_CS_PIN) = 1;

    dly = 50;
    while(--dly);

    spi_set_speed(f_spi);

    return status;
}

TMC_spi_status_t tmc_spi_write (trinamic_motor_t driver, TMC_spi_datagram_t *reg)
{
    TMC_spi_status_t status = 0;

    uint8_t res;
    uint_fast8_t idx = N_AXIS, ridx = 0;
    uint32_t f_spi = spi_set_speed(SPI_BAUDRATEPRESCALER_32);
    volatile uint32_t dly = 100;

    memcpy(&datagram[driver.axis], reg, sizeof(TMC_spi_datagram_t));
    datagram[driver.axis].addr.write = 1;

    BITBAND_PERI(TRINAMIC_CS_PORT->ODR, TRINAMIC_CS_PIN) = 0;

    do {
        ridx++;
        if(bit_istrue(tmc.mask, bit(--idx))) {

            res = spi_put_byte(datagram[idx].addr.value);
            spi_put_byte(datagram[idx].payload.data[3]);
            spi_put_byte(datagram[idx].payload.data[2]);
            spi_put_byte(datagram[idx].payload.data[1]);
            spi_put_byte(datagram[idx].payload.data[0]);

            if(N_AXIS - ridx == driver.axis)
                status = res;

            if(idx == driver.axis) {
                datagram[idx].addr.idx = 0; //TMC_SPI_STATUS_REG;
                datagram[idx].addr.write = 0;
            }
        }
    } while(idx);

    while(--dly);

    BITBAND_PERI(TRINAMIC_CS_PORT->ODR, TRINAMIC_CS_PIN) = 1;

    dly = 50;
    while(--dly);

    spi_set_speed(f_spi);

    return status;
}

void TMC_SPI_DriverInit (axes_signals_t axisflags)
{
    tmc = axisflags;
    n_axis = 0;
    while(axisflags.mask) {
        n_axis += (axisflags.mask & 0x01);
        axisflags.mask >>= 1;
    }
}

#endif


void board_init (void)
{

    GPIO_InitTypeDef GPIO_Init = {0};

    GPIO_Init.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_Init.Mode = GPIO_MODE_OUTPUT_PP;

#if TRINAMIC_ENABLE == 2130 || TRINAMIC_ENABLE == 5160

    trinamic_driver_if_t driver = {
        .on_drivers_init = TMC_SPI_DriverInit
    };

    spi_init();
    GPIO_Init.Pin = TRINAMIC_CS_BIT;
    HAL_GPIO_Init(TRINAMIC_CS_PORT, &GPIO_Init);
    BITBAND_PERI(TRINAMIC_CS_PORT->ODR, TRINAMIC_CS_PIN) = 1;

    uint_fast8_t idx = N_AXIS;
    do {
        datagram[--idx].addr.idx = 0; //TMC_SPI_STATUS_REG;
    } while(idx);

    trinamic_if_init(&driver);

#endif

}

#endif
