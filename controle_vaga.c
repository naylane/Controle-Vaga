/*
 *  Por: Naylane do Nascimento Ribeiro
 *  Data: 26/05/2025
 */

#include "controle_vaga.h"

ssd1306_t ssd;
SemaphoreHandle_t xContadorSem;
SemaphoreHandle_t xSemaforoSaida;
SemaphoreHandle_t xSemaforoReset;
SemaphoreHandle_t xDisplayMutex;
uint16_t eventosProcessados = 0;
uint MAX = 10; // Número máximo de vagas no estacionamento
uint vagas_preenchidas = 0;

void vTaskEntrada(void *params);
void vTaskSaida(void *params);
void vTaskReset(void *params);
void vTaskLeds(void *params);
void gpio_irq_handler(uint gpio, uint32_t events);
void init_gpio_button(uint gpio);
void init_gpio_led(uint gpio);


int main() {
    stdio_init_all();

    // Configura clock do sistema
    if (set_sys_clock_khz(128000, false)) {
        printf("Configuração do clock do sistema completa!\n");
    } else {
        printf("Configuração do clock do sistema falhou!\n");
        return -1;
    }

    // Inicialização do display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Configuração do buzzer
    buzzer_setup_pwm(BUZZER_PIN, 4000);

    // Configura os botões
    init_gpio_button(BUTTON_A);
    init_gpio_button(BUTTON_B);
    init_gpio_button(BUTTON_JOY);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_JOY, GPIO_IRQ_EDGE_FALL, true);

    // Cria semáforos de contagem, semáforo binário e mutex
    xContadorSem = xSemaphoreCreateCounting(10, 0);
    xSemaforoSaida = xSemaphoreCreateCounting(10, 0);
    xSemaforoReset = xSemaphoreCreateBinary();
    xDisplayMutex = xSemaphoreCreateMutex();

    // Cria tarefas
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "SaidaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "ResetTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskLeds, "LedsTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}


// Preenche uma vaga no estacionamento
void vTaskEntrada(void *params) {
    char buffer[32];

    // Tela inicial
    if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
        ssd1306_fill(&ssd, 0);
        ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
        ssd1306_draw_string(&ssd, "  evento...", 5, 34);
        ssd1306_send_data(&ssd);
        xSemaphoreGive(xDisplayMutex);
    }

    while (true) {
        // Aguarda semáforo (um evento)
        if (xSemaphoreTake(xContadorSem, portMAX_DELAY) == pdTRUE) {
            if (eventosProcessados == MAX) {
                buzzer_play(BUZZER_PIN, 1, 1000, 500);
            } else {
                eventosProcessados++;
            }

            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
                // Atualiza display com a nova contagem
                ssd1306_fill(&ssd, 0);
                sprintf(buffer, "Eventos: %d", eventosProcessados);
                ssd1306_draw_string(&ssd, "Evento ", 5, 10);
                ssd1306_draw_string(&ssd, "recebido!", 5, 19);
                ssd1306_draw_string(&ssd, buffer, 5, 44);
                ssd1306_send_data(&ssd);
                xSemaphoreGive(xDisplayMutex);
            }

            // Simula tempo de processamento
            vTaskDelay(pdMS_TO_TICKS(1500));

            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
                // Retorna à tela de espera
                ssd1306_fill(&ssd, 0);
                ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
                ssd1306_draw_string(&ssd, "  evento...", 5, 34);
                ssd1306_send_data(&ssd);
                xSemaphoreGive(xDisplayMutex);
            }
        }
    }
}


// Libera uma vaga no estacionamento
void vTaskSaida(void *params) {
    char buffer[32];

    while (true) {
        if (xSemaphoreTake(xSemaforoSaida, portMAX_DELAY) == pdTRUE) {
            if (eventosProcessados > 0) {
                eventosProcessados--;
                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
                    ssd1306_fill(&ssd, 0);
                    sprintf(buffer, "Eventos: %d", eventosProcessados);
                    ssd1306_draw_string(&ssd, "Saida!", 5, 10);
                    ssd1306_draw_string(&ssd, buffer, 5, 44);
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xDisplayMutex);
                }

                vTaskDelay(pdMS_TO_TICKS(1500));

                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
                    ssd1306_fill(&ssd, 0);
                    ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
                    ssd1306_draw_string(&ssd, "  evento...", 5, 34);
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xDisplayMutex);
                }
            }
        }
    }
}


// Esvazia (reseta) o estacionamento.
void vTaskReset(void *params) {
    char buffer[32];

    while (true) {
        if (xSemaphoreTake(xSemaforoReset, portMAX_DELAY) == pdTRUE) {
            //buzzer_play(BUZZER_PIN, 2, 500, 500);
            eventosProcessados = 0;
            vagas_preenchidas = 0;

            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
                ssd1306_fill(&ssd, 0);
                ssd1306_draw_string(&ssd, "Contador ", 5, 10);
                ssd1306_draw_string(&ssd, "resetado!", 5, 19);
                sprintf(buffer, "Eventos: %d", eventosProcessados);
                ssd1306_draw_string(&ssd, buffer, 5, 44);
                ssd1306_send_data(&ssd);
                xSemaphoreGive(xDisplayMutex);
            }

            vTaskDelay(pdMS_TO_TICKS(1500));

            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
                // Retorna à tela de espera
                ssd1306_fill(&ssd, 0);
                ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
                ssd1306_draw_string(&ssd, "  evento...", 5, 34);
                ssd1306_send_data(&ssd);
                xSemaphoreGive(xDisplayMutex);
            }
        }
    }
}


// Controla os LEDs RGB de acordo com a quantidade de vagas ocupadas.
void vTaskLeds(void *params) {
    // Configura os LEDs
    init_gpio_led(LED_RED_PIN);
    init_gpio_led(LED_BLUE_PIN);
    init_gpio_led(LED_GREEN_PIN);

    while (true) {
        // Liga o LED azul se não tiver vagas ocupadas
        if (eventosProcessados == 0) {
            gpio_put(LED_RED_PIN, 0);
            gpio_put(LED_GREEN_PIN, 0);
            gpio_put(LED_BLUE_PIN, 1);
        }
        // Liga o LED verde se houver vagas ocupadas dentro da faixa
        else if (eventosProcessados < MAX-2 && eventosProcessados > 0) {
            gpio_put(LED_RED_PIN, 0);
            gpio_put(LED_GREEN_PIN, 1);
            gpio_put(LED_BLUE_PIN, 0);
        }
        // Liga o LED amarelo se houver apenas uma vaga restante
        else if (eventosProcessados == MAX-1) {
            gpio_put(LED_RED_PIN, 1);
            gpio_put(LED_GREEN_PIN, 1);
            gpio_put(LED_BLUE_PIN, 0);
        }
        // Liga o LED vermelho se o contador de eventos for igual a MAX
        else if (eventosProcessados == MAX) {
            gpio_put(LED_RED_PIN, 1);
            gpio_put(LED_GREEN_PIN, 0);
            gpio_put(LED_BLUE_PIN, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


// Função de tratamento de interrupção dos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (gpio == BUTTON_B) {
        if (current_time - last_time_B > DEBOUNCE_TIME) {
            reset_usb_boot(0, 0);
            /*
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemaforoSaida, &xHigherPriorityTaskWoken);
            last_time_B = current_time;
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);*/
            return;
        }
    }
    else if (gpio == BUTTON_A) {
        if (current_time - last_time_A > DEBOUNCE_TIME) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xContadorSem, &xHigherPriorityTaskWoken);
            last_time_A = current_time;
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            return;
        }
    }
    else if (gpio == BUTTON_JOY) {
        if (current_time - last_time_joy > DEBOUNCE_TIME) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemaforoReset, &xHigherPriorityTaskWoken);
            last_time_joy = current_time;
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            return;
        }
    }
}


// Inicializa GPIO para LEDs RGB
void init_gpio_led(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
}


// Inicializa GPIO para botões
void init_gpio_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}