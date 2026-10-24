#pragma once
// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#define __STATIC_INLINE static inline
#define __STATIC_FORCEINLINE static inline __attribute__((always_inline))
#define __WEAK __attribute__((weak))
#define __NOP() __asm__ volatile("nop")
#define CPU_CLOCK 360000000U
#define IO_PORT_WRITE_CYCLES 32U
#define DAP_SWD 1
#define DAP_JTAG 0
#define DAP_JTAG_DEV_CNT 1U
#define DAP_DEFAULT_PORT 1U
#define DAP_DEFAULT_SWJ_CLOCK 100000U
#define DAP_PACKET_SIZE 1024U
#define DAP_PACKET_COUNT 1U
#define SWO_UART 0
#define SWO_MANCHESTER 0
#define SWO_STREAM 0
#define TIMESTAMP_CLOCK 1000000U
#define DAP_UART 0
#define DAP_UART_USB_COM_PORT 0
#define SWCLK 20
#define SWDIO 21
#define NRESET 22

// UART0 is wired to the board USB-UART bridge on the tested ESP32-P4 board.
#define P4_UART_TX 37
#define P4_UART_RX 38
#define P4_UART_BAUD 921600U
static inline uint8_t info_string(char *s, const char *v) {strcpy(s,v); return strlen(v)+1;}
static inline uint8_t DAP_GetVendorString(char *s) {return info_string(s,"P4 Lab");}
static inline uint8_t DAP_GetProductString(char *s) {return info_string(s,"P4 SWD UART Probe");}
static inline uint8_t DAP_GetSerNumString(char *s) {(void)s; return 0;}
static inline uint8_t DAP_GetTargetDeviceVendorString(char *s) {(void)s; return 0;}
static inline uint8_t DAP_GetTargetDeviceNameString(char *s) {(void)s; return 0;}
static inline uint8_t DAP_GetTargetBoardVendorString(char *s) {(void)s; return 0;}
static inline uint8_t DAP_GetTargetBoardNameString(char *s) {(void)s; return 0;}
static inline uint8_t DAP_GetProductFirmwareVersionString(char *s) {return info_string(s,"0.1.0-experimental");}
static inline void PORT_OFF(void) {
  gpio_set_direction(SWCLK,GPIO_MODE_INPUT); gpio_set_direction(SWDIO,GPIO_MODE_INPUT);
  gpio_set_direction(NRESET,GPIO_MODE_INPUT);
  gpio_set_pull_mode(SWCLK,GPIO_FLOATING); gpio_set_pull_mode(SWDIO,GPIO_FLOATING);
  gpio_set_pull_mode(NRESET,GPIO_FLOATING);
}
static inline void DAP_SETUP(void) {
  gpio_reset_pin(SWCLK); gpio_reset_pin(SWDIO); gpio_reset_pin(NRESET); PORT_OFF();
}
static inline void PORT_SWD_SETUP(void) {
  gpio_set_level(SWCLK,1); gpio_set_level(SWDIO,1); gpio_set_level(NRESET,1);
  gpio_set_direction(SWCLK,GPIO_MODE_INPUT_OUTPUT);
  gpio_set_direction(SWDIO,GPIO_MODE_INPUT_OUTPUT);
  gpio_set_pull_mode(SWDIO,GPIO_PULLUP_ONLY);
  gpio_set_direction(NRESET,GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_pull_mode(NRESET,GPIO_PULLUP_ONLY);
}
static inline uint32_t PIN_SWCLK_TCK_IN(void) {return gpio_get_level(SWCLK);}
static inline void PIN_SWCLK_TCK_SET(void) {gpio_set_level(SWCLK,1);}
static inline void PIN_SWCLK_TCK_CLR(void) {gpio_set_level(SWCLK,0);}
static inline uint32_t PIN_SWDIO_TMS_IN(void) {return gpio_get_level(SWDIO);}
static inline void PIN_SWDIO_TMS_SET(void) {gpio_set_level(SWDIO,1);}
static inline void PIN_SWDIO_TMS_CLR(void) {gpio_set_level(SWDIO,0);}
static inline uint32_t PIN_SWDIO_IN(void) {return gpio_get_level(SWDIO);}
static inline void PIN_SWDIO_OUT(uint32_t b) {gpio_set_level(SWDIO,b&1);}
static inline void PIN_SWDIO_OUT_ENABLE(void) {gpio_set_direction(SWDIO,GPIO_MODE_INPUT_OUTPUT);}
static inline void PIN_SWDIO_OUT_DISABLE(void) {gpio_set_direction(SWDIO,GPIO_MODE_INPUT);}
static inline uint32_t PIN_nRESET_IN(void) {return gpio_get_level(NRESET);}
static inline void PIN_nRESET_OUT(uint32_t b) {gpio_set_level(NRESET,b&1);}
// SWD only: unwired JTAG signals are never driven.
static inline uint32_t PIN_TDI_IN(void) {return 0;}
static inline uint32_t PIN_TDO_IN(void) {return 0;}
static inline uint32_t PIN_nTRST_IN(void) {return 1;}
static inline void PIN_TDI_OUT(uint32_t b) {(void)b;}
static inline void PIN_nTRST_OUT(uint32_t b) {(void)b;}
static inline void LED_CONNECTED_OUT(uint32_t b) {(void)b;}
static inline void LED_RUNNING_OUT(uint32_t b) {(void)b;}
static inline uint32_t TIMESTAMP_GET(void) {return esp_timer_get_time();}
static inline uint8_t RESET_TARGET(void) {return 0;}
