/*
 *  Por: Naylane do Nascimento Ribeiro
 *  Data: 26/05/2025
 */

#include "controle_vaga.h"

ssd1306_t ssd;
SemaphoreHandle_t xContadorSem;
uint16_t eventosProcessados = 0;

void gpio_callback(uint gpio, uint32_t events);
void vContadorTask(void *params);
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
    buzzer_play(BUZZER_PIN, 1, 1000, 500);  // Toca um som inicial

    // Configura os botões
    init_gpio_button(BUTTON_A);
    init_gpio_button(BUTTON_B);
    init_gpio_button(BUTTON_JOY);

    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_JOY, GPIO_IRQ_EDGE_FALL, true);

    // Cria semáforo de contagem (máximo 10, inicial 0)
    xContadorSem = xSemaphoreCreateCounting(10, 0);

    // Cria tarefa
    xTaskCreate(vContadorTask, "ContadorTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    //xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

// ISR do botão A (incrementa o semáforo de contagem)
void gpio_callback(uint gpio, uint32_t events) {
    //buzzer_play(BUZZER_PIN, 1, 1000, 100);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xContadorSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Tarefa que consome eventos e mostra no display
void vContadorTask(void *params) {
    char buffer[32];

    // Tela inicial
    ssd1306_fill(&ssd, 0);
    ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
    ssd1306_draw_string(&ssd, "  evento...", 5, 34);
    ssd1306_send_data(&ssd);

    while (true) {
        // Aguarda semáforo (um evento)
        if (xSemaphoreTake(xContadorSem, portMAX_DELAY) == pdTRUE) {
            eventosProcessados++;
            if (eventosProcessados > 10) {
                eventosProcessados = 1; // Reseta contagem se passar de 10
            }

            // Atualiza display com a nova contagem
            ssd1306_fill(&ssd, 0);
            sprintf(buffer, "Eventos: %d", eventosProcessados);
            ssd1306_draw_string(&ssd, "Evento ", 5, 10);
            ssd1306_draw_string(&ssd, "recebido!", 5, 19);
            ssd1306_draw_string(&ssd, buffer, 5, 44);
            ssd1306_send_data(&ssd);

            // Simula tempo de processamento
            vTaskDelay(pdMS_TO_TICKS(1500));

            // Retorna à tela de espera
            ssd1306_fill(&ssd, 0);
            ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
            ssd1306_draw_string(&ssd, "  evento...", 5, 34);
            ssd1306_send_data(&ssd);
        }
    }
}

// ISR para BOOTSEL e botão de evento
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (gpio == BUTTON_B) {
        if (current_time - last_time_B > DEBOUNCE_TIME) {
            last_time_B = current_time;
            return;
        }
    }
    else if (gpio == BUTTON_A) {
        if (current_time - last_time_A > DEBOUNCE_TIME) {
            gpio_callback(gpio, events);
            last_time_A = current_time;
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

void init_gpio_button(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}