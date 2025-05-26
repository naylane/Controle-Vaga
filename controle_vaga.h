#ifndef CONTROLE_VAGA_H
#define CONTROLE_VAGA_H

#include "stdio.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lib/ssd1306.h"
#include "lib/buzzer.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

#define BUTTON_A 5
#define BUTTON_B 6
#define BUTTON_JOY 22
#define DEBOUNCE_TIME 300000        // Tempo para debounce em ms
static uint32_t last_time_A = 0;    // Tempo da última interrupção do botão A
static uint32_t last_time_B = 0;    // Tempo da última interrupção do botão B
static uint32_t last_time_joy = 0;    // Tempo da última interrupção do botão B

// Definição dos pinos dos LEDs
#define LED_RED_PIN 13
#define LED_BLUE_PIN 12      
#define LED_GREEN_PIN 11

#endif