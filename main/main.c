// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "DAP_config.h"
#include "DAP.h"

// Reuse the OpenOCD CMSIS-DAP TCP frame layout. The host bridge binds to loopback only.
static bool read_exact(uint8_t *p, size_t n) {
  int64_t until=esp_timer_get_time()+500000;
  while(n && esp_timer_get_time()<until) {
    int got=uart_read_bytes(UART_NUM_0,p,n,pdMS_TO_TICKS(20));
    if(got>0) {p+=got; n-=got;}
  }
  return n==0;
}
static uint16_t u16(const uint8_t *p) {return p[0]|(p[1]<<8);}
static uint32_t u32(const uint8_t *p) {return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24);}

// The ARM command handler assumes valid packets. Validate length and response bounds first.
static bool valid(const uint8_t *p, size_t n) {
  if(!n) return false;
  switch(p[0]) {
    case 0x00: case 0x02: case 0x13: return n==2;
    case 0x01: return n==3;
    case 0x03: case 0x07: case 0x0a: case 0x80: case 0x81: return n==1;
    case 0x04: return n==6 && u16(p+2)<=100 && u16(p+4)<=100;
    case 0x08: return n==6;
    case 0x09: return n==3 && u16(p+1)<=1000;
    case 0x10: return n==7 && u32(p+3)<=1000;
    case 0x11: return n==5 && u32(p+1)>=10000 && u32(p+1)<=500000;
    case 0x12: return n>=2 && n==2+((p[1]?p[1]:256)+7)/8;
    case 0x05: {
      if(n<3) return false;
      size_t pos=3, response=3;
      for(int i=0;i<p[2];i++) {
        if(pos>=n) return false;
        uint8_t req=p[pos++];
        if(req&0xc0) return false;
        if(!(req&2)||(req&0x30)) pos+=4;
        else response+=4;
        if(req&0x80) response+=4;
      }
      return pos==n && response<=1024;
    }
    case 0x06: {
      if(n<5 || (p[4]&0xf0)) return false;
      unsigned count=u16(p+2);
      if(p[4]&2) return n==5 && count<=255;
      return count<=254 && n==5+count*4;
    }
    case 0x1d: {
      if(n<2) return false;
      size_t pos=2, response=2;
      for(int i=0;i<p[1];i++) {
        if(pos>=n) return false;
        unsigned info=p[pos++], size=((info&63)?(info&63):64)+7;
        if(info&0x80) response+=size/8; else pos+=size/8;
      }
      return pos==n && response<=1024;
    }
    default: return false;
  }
}

static uint8_t selftest(void) {
  // Checks each pin's input buffer against its own output; does not emulate a target MCU.
  const int pins[]={SWCLK,SWDIO,NRESET};
  uint8_t failures=0;
  PORT_SWD_SETUP();
  for(int i=0;i<3;i++) {
    for(int level=0;level<2;level++) {
      gpio_set_level(pins[i],level); esp_rom_delay_us(30);
      if(gpio_get_level(pins[i])!=level) failures|=1<<(i*2+level);
    }
  }
  PORT_OFF(); DAP_Data.debug_port=0;
  return failures;
}

void app_main(void) {
  DAP_Setup();
  uart_config_t cfg={.baud_rate=P4_UART_BAUD,.data_bits=UART_DATA_8_BITS,
    .parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,
    .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,8192,8192,0,NULL,0));
  ESP_ERROR_CHECK(uart_param_config(UART_NUM_0,&cfg));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0,P4_UART_TX,P4_UART_RX,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
  uint32_t packets=0; int64_t last=esp_timer_get_time();
  uint8_t hdr[8],req[1040],resp[1040];
  for(;;) {
    if(!read_exact(hdr,1)) {
      if(esp_timer_get_time()-last>5000000) {PORT_OFF();DAP_Data.debug_port=0;}
      continue;
    }
    if(hdr[0]!='D') continue;
    if(!read_exact(hdr+1,7)) {PORT_OFF();DAP_Data.debug_port=0;continue;}
    size_t len=u16(hdr+4);
    if(memcmp(hdr,"DAP\0",4)||hdr[6]!=1||hdr[7]||len<1||len>1024) continue;
    if(!read_exact(req,len)) {PORT_OFF();DAP_Data.debug_port=0;continue;}
    memset(resp,0,sizeof(resp)); size_t out=1;
    if(!valid(req,len)) resp[0]=0xff;
    else if(req[0]==0x80) {
      esp_chip_info_t info;esp_chip_info(&info);uint32_t flash=0;
      esp_flash_get_size(NULL,&flash);
      resp[0]=0x80;resp[1]=1;resp[2]=info.revision;resp[3]=info.revision>>8;
      memcpy(resp+4,&flash,4);memcpy(resp+8,&packets,4);
      resp[12]=SWCLK;resp[13]=SWDIO;resp[14]=NRESET;out=15;
    } else if(req[0]==0x81) {resp[0]=0x81;resp[1]=selftest();out=2;}
    else {
      // GPIO operations are only allowed after an explicit DAP_Connect.
      bool needs_port=req[0]==0x05||req[0]==0x06||req[0]==0x08||req[0]==0x10||req[0]==0x12||req[0]==0x1d;
      if(needs_port&&!DAP_Data.debug_port) resp[0]=0xff;
      else out=DAP_ExecuteCommand(req,resp)&0xffff;
      // This experimental build does not implement batch commands; clear that capability bit.
      if(req[0]==0 && req[1]==0xf0 && out>=3) resp[2]&=~(1U<<4);
    }
    if(out>1024||out==0) {out=1;resp[0]=0xff;}
    hdr[4]=(uint8_t)out;hdr[5]=(uint8_t)(out>>8);hdr[6]=2;hdr[7]=0;
    uart_write_bytes(UART_NUM_0,hdr,8);uart_write_bytes(UART_NUM_0,resp,out);
    packets++;last=esp_timer_get_time();
  }
}
