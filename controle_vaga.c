/*
 *  Por: Naylane do Nascimento Ribeiro
 *  Data: 26/05/2025
 */

#include "controle_vaga.h"

ssd1306_t ssd;
SemaphoreHandle_t xContadorSem;
SemaphoreHandle_t xSemaforoSaida;
SemaphoreHandle_t xDisplayMutex;
uint16_t eventosProcessados = 0;
uint MAX = 10;
uint vagas_preenchidas = 0;

void vTaskEntrada(void *params);
void vTaskSaida(void *params);
void gpio_irq_handler(uint gpio, uint32_t events);
void init_gpio_button(uint gpio);


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
    buzzer_setup_pwm(BUZZER_PIN, 4000);     // Configura o buzzer para 4kHz
    //buzzer_play(BUZZER_PIN, 1, 1000, 500);  // Toca um som inicial

    // Configura os botões
    init_gpio_button(BUTTON_A);
    init_gpio_button(BUTTON_B);
    init_gpio_button(BUTTON_JOY);

    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_JOY, GPIO_IRQ_EDGE_FALL, true);

    // Cria semáforos de contagem e mutex
    xContadorSem = xSemaphoreCreateCounting(10, 0);
    xSemaforoSaida = xSemaphoreCreateCounting(10, 0);
    xDisplayMutex = xSemaphoreCreateMutex();


    // Cria tarefa
    xTaskCreate(vTaskEntrada, "ContadorTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "SaidaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}


// Tarefa que consome eventos e mostra no display
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
            eventosProcessados++;
            if (eventosProcessados > MAX) {
                buzzer_play(BUZZER_PIN, 1, 1000, 500);
                eventosProcessados = 1; // Reseta contagem se passar de 10
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


// Tarefa que consome eventos de saída e mostra no display
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
                //buzzer_play(BUZZER_PIN, 1, 1000, 100);
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


// ISR para BOOTSEL e botão de evento
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (gpio == BUTTON_B) {
        if (current_time - last_time_B > DEBOUNCE_TIME) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemaforoSaida, &xHigherPriorityTaskWoken); // Sinaliza saída
            last_time_B = current_time;
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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
            buzzer_play(BUZZER_PIN, 1, 1000, 100); // Toca som de boot
            reset_usb_boot(0, 0);
            last_time_joy = current_time;
            return;
        }
    }
}


// Inicializa GPIO para botões
void init_gpio_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}