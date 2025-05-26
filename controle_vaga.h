#ifndef CONTROLE_VAGA_H
#define CONTROLE_VAGA_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "pico/bootrom.h"
#include "stdio.h"
#include "lib/buzzer.h"
//#include "hardware/pio.h"
//#include "lib/ws2812.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

#define BUTTON_A 5
#define BUTTON_B 6
#define BUTTON_JOY 22
#define DEBOUNCE_TIME 200000        // Tempo para debounce em ms
static uint32_t last_time_A = 0;    // Tempo da última interrupção do botão A
static uint32_t last_time_B = 0;    // Tempo da última interrupção do botão B
static uint32_t last_time_joy = 0;    // Tempo da última interrupção do botão B

#define WS2812_PIN 7
//extern PIO pio;
//extern uint sm;

#endif