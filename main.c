#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdio.h>

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

// Definir dimensões do display (ajustável)
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64 // Altere para 32 se o display for 128x32

// Pinos para LED RGB
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12

// Pino para a matriz de LEDs WS2812
#define WS2812_PIN 7
#define NUM_LEDS 64 // Matriz 8x8 da BitDog Lab

// Pinos para os buzzers
#define BUZZER1 10
#define BUZZER2 21

// Pino para o botão A
#define BUTTON_A 5

// Modos
#define MODE_NORMAL 0
#define MODE_NOTURNO 1
#define MODE_ALTO_FLUXO 2
#define MODE_BAIXO_FLUXO 3

// Variável global para o modo atual
static volatile uint8_t current_mode = MODE_NORMAL;

// Fase atual (0 = Verde, 1 = Amarelo, 2 = Vermelho, 3 = Amarelo Piscante Aceso, 4 = Amarelo Piscante Apagado)
static volatile uint8_t current_phase = 0;

// Tempo restante no ciclo atual (em milissegundos)
static volatile uint32_t time_remaining_ms = 20000;

// Funções auxiliares para WS2812
static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b; // Ordem GRB
}

static void ws2812_init(PIO pio, uint sm, uint pin) {
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, pin, 800000, false); // false = RGB, não RGBW
}

static void ws2812_set_color(PIO pio, uint sm, uint32_t color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, color << 8u); // Adiciona deslocamento para alinhar os dados
    }
}

// Tarefa para monitorar o botão A e alternar o modo
void vButtonATask(void *pvParameters) {
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    bool last_state = true; // Estado inicial do botão (considerando pull-up)
    while (true) {
        bool current_state = gpio_get(BUTTON_A);
        if (last_state && !current_state) { // Detecção de borda de descida (botão pressionado)
            current_mode = (current_mode + 1) % 4; // Alterna entre 0 (Normal), 1 (Noturno), 2 (Alto Fluxo), 3 (Baixo Fluxo)
        }
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(50)); // Debounce simples
    }
}

// Tarefa para controlar a matriz de LEDs WS2812 (tarefa "mestre")
void vMatrixLedTask(void *pvParameters) {
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true); // Aloca uma máquina de estado disponível
    ws2812_init(pio, sm, WS2812_PIN);

    while (true) {
        if (current_mode == MODE_NORMAL) {
            // Modo Normal: Verde (20s) -> Amarelo (3s) -> Vermelho (20s) -> Verde
            current_phase = 0; // Verde
            for (int i = 0; i < 200; i++) { // 20 segundos (200 * 100ms)
                time_remaining_ms = (200 - i) * 100; // Atualiza tempo restante
                ws2812_set_color(pio, sm, rgb_to_grb(0, 10, 0)); // Verde
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_NORMAL) break;
            }
            current_phase = 1; // Amarelo
            for (int i = 0; i < 30; i++) { // 3 segundos (30 * 100ms)
                time_remaining_ms = 0; // Contador zerado para amarelo
                ws2812_set_color(pio, sm, rgb_to_grb(10, 10, 0)); // Amarelo
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_NORMAL) break;
            }
            current_phase = 2; // Vermelho
            for (int i = 0; i < 200; i++) { // 20 segundos (200 * 100ms)
                time_remaining_ms = (200 - i) * 100; // Atualiza tempo restante
                ws2812_set_color(pio, sm, rgb_to_grb(10, 0, 0)); // Vermelho
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_NORMAL) break;
            }
        } else if (current_mode == MODE_NOTURNO) {
            // Modo Noturno: Amarelo piscando lentamente (restaurado para 0.5s aceso, 1.5s apagado)
            current_phase = 3; // Amarelo Piscante Aceso
            for (int i = 0; i < 5; i++) { // 0.5s aceso (5 * 100ms)
                time_remaining_ms = (5 - i) * 100; // 500ms a 0ms
                ws2812_set_color(pio, sm, rgb_to_grb(10, 10, 0)); // Amarelo
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_NOTURNO) break;
            }
            current_phase = 4; // Amarelo Piscante Apagado
            for (int i = 0; i < 15; i++) { // 1.5s apagado (15 * 100ms)
                time_remaining_ms = (15 - i) * 100; // 1500ms a 0ms
                ws2812_set_color(pio, sm, rgb_to_grb(0, 0, 0)); // Desligado
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_NOTURNO) break;
            }
        } else if (current_mode == MODE_ALTO_FLUXO) {
            // Modo Alto Fluxo: Verde (25s) -> Amarelo (3s) -> Vermelho (15s) -> Verde
            current_phase = 0; // Verde
            for (int i = 0; i < 250; i++) { // 25 segundos (250 * 100ms)
                time_remaining_ms = (250 - i) * 100; // Atualiza tempo restante
                ws2812_set_color(pio, sm, rgb_to_grb(0, 10, 0)); // Verde
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_ALTO_FLUXO) break;
            }
            current_phase = 1; // Amarelo
            for (int i = 0; i < 30; i++) { // 3 segundos (30 * 100ms)
                time_remaining_ms = 0; // Contador zerado para amarelo
                ws2812_set_color(pio, sm, rgb_to_grb(10, 10, 0)); // Amarelo
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_ALTO_FLUXO) break;
            }
            current_phase = 2; // Vermelho
            for (int i = 0; i < 150; i++) { // 15 segundos (150 * 100ms)
                time_remaining_ms = (150 - i) * 100; // Atualiza tempo restante
                ws2812_set_color(pio, sm, rgb_to_grb(10, 0, 0)); // Vermelho
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_ALTO_FLUXO) break;
            }
        } else if (current_mode == MODE_BAIXO_FLUXO) {
            // Modo Baixo Fluxo: Vermelho (25s) -> Amarelo (3s) -> Verde (15s) -> Vermelho
            current_phase = 2; // Vermelho
            for (int i = 0; i < 250; i++) { // 25 segundos (250 * 100ms)
                time_remaining_ms = (250 - i) * 100; // Atualiza tempo restante
                ws2812_set_color(pio, sm, rgb_to_grb(10, 0, 0)); // Vermelho
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_BAIXO_FLUXO) break;
            }
            current_phase = 1; // Amarelo
            for (int i = 0; i < 30; i++) { // 3 segundos (30 * 100ms)
                time_remaining_ms = 0; // Contador zerado para amarelo
                ws2812_set_color(pio, sm, rgb_to_grb(10, 10, 0)); // Amarelo
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_BAIXO_FLUXO) break;
            }
            current_phase = 0; // Verde
            for (int i = 0; i < 150; i++) { // 15 segundos (150 * 100ms)
                time_remaining_ms = (150 - i) * 100; // Atualiza tempo restante
                ws2812_set_color(pio, sm, rgb_to_grb(0, 10, 0)); // Verde
                vTaskDelay(pdMS_TO_TICKS(100));
                if (current_mode != MODE_BAIXO_FLUXO) break;
            }
        }
    }
}

// Tarefa para controlar o LED RGB
void vRgbLedTask(void *pvParameters) {
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);

    // Pequeno atraso inicial para sincronizar com vMatrixLedTask
    vTaskDelay(pdMS_TO_TICKS(10));

    while (true) {
        switch (current_phase) {
            case 0: // Verde
                gpio_put(LED_RED, false);
                gpio_put(LED_GREEN, true);
                gpio_put(LED_BLUE, false); // Verde
                break;
            case 1: // Amarelo (modo normal, alto fluxo, baixo fluxo)
                gpio_put(LED_RED, true);
                gpio_put(LED_GREEN, true);
                gpio_put(LED_BLUE, false); // Amarelo
                break;
            case 2: // Vermelho
                gpio_put(LED_RED, true);
                gpio_put(LED_GREEN, false);
                gpio_put(LED_BLUE, false); // Vermelho
                break;
            case 3: // Amarelo Piscante Aceso (modo noturno)
                gpio_put(LED_RED, true);
                gpio_put(LED_GREEN, true);
                gpio_put(LED_BLUE, false); // Amarelo
                break;
            case 4: // Amarelo Piscante Apagado (modo noturno)
                gpio_put(LED_RED, false);
                gpio_put(LED_GREEN, false);
                gpio_put(LED_BLUE, false); // Desligado
                break;
            default:
                gpio_put(LED_RED, false);
                gpio_put(LED_GREEN, false);
                gpio_put(LED_BLUE, false); // Desligado
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Sincroniza com o mesmo intervalo de vMatrixLedTask
    }
}

// Tarefa para controlar os buzzers
void vBuzzerTask(void *pvParameters) {
    gpio_init(BUZZER1);
    gpio_init(BUZZER2);
    gpio_set_dir(BUZZER1, GPIO_OUT);
    gpio_set_dir(BUZZER2, GPIO_OUT);

    while (true) {
        if (current_mode == MODE_NORMAL) {
            // Modo Normal
            // Verde: 1 beep curto por segundo (pode atravessar)
            for (int i = 0; i < 20; i++) { // 20 segundos
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(200)); // Beep curto
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(800)); // Intervalo (1s total por ciclo)
                if (current_mode != MODE_NORMAL) break;
            }
            // Amarelo: Beep rápido intermitente (atenção)
            for (int i = 0; i < 7; i++) { // 3 segundos (7 ciclos de ~428ms)
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(200)); // Beep rápido
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(228)); // Intervalo ajustado para 3s totais
                if (current_mode != MODE_NORMAL) break;
            }
            // Vermelho: Tom contínuo curto (500ms ligado, 1.5s desligado) (pare)
            for (int i = 0; i < 10; i++) { // 20 segundos (10 ciclos de 2s)
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(500)); // Tom contínuo
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(1500)); // Intervalo
                if (current_mode != MODE_NORMAL) break;
            }
        } else if (current_mode == MODE_NOTURNO) {
            // Modo Noturno: Beep lento a cada 2s (restaurado para 0.2s aceso, 1.8s apagado)
            gpio_put(BUZZER1, true);
            gpio_put(BUZZER2, true);
            vTaskDelay(pdMS_TO_TICKS(200)); // Beep curto
            gpio_put(BUZZER1, false);
            gpio_put(BUZZER2, false);
            vTaskDelay(pdMS_TO_TICKS(1800)); // Intervalo de 2s
            if (current_mode != MODE_NOTURNO) continue; // Garante que não saia do modo noturno
        } else if (current_mode == MODE_ALTO_FLUXO) {
            // Modo Alto Fluxo
            // Verde: 1 beep curto por segundo (pode atravessar)
            for (int i = 0; i < 25; i++) { // 25 segundos
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(200)); // Beep curto
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(800)); // Intervalo (1s total por ciclo)
                if (current_mode != MODE_ALTO_FLUXO) break;
            }
            // Amarelo: Beep rápido intermitente (atenção)
            for (int i = 0; i < 7; i++) { // 3 segundos (7 ciclos de ~428ms)
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(200)); // Beep rápido
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(228)); // Intervalo ajustado para 3s totais
                if (current_mode != MODE_ALTO_FLUXO) break;
            }
            // Vermelho: Tom contínuo curto (500ms ligado, 1.5s desligado) (pare)
            for (int i = 0; i < 7; i++) { // 15 segundos (7 ciclos de ~2.14s)
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(500)); // Tom contínuo
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(1643)); // Intervalo ajustado para 15s totais
                if (current_mode != MODE_ALTO_FLUXO) break;
            }
        } else if (current_mode == MODE_BAIXO_FLUXO) {
            // Modo Baixo Fluxo
            // Vermelho: Tom contínuo curto (500ms ligado, 1.5s desligado) (pare)
            for (int i = 0; i < 12; i++) { // 25 segundos (12 ciclos de ~2.08s)
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(500)); // Tom contínuo
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(1583)); // Intervalo ajustado para 25s totais
                if (current_mode != MODE_BAIXO_FLUXO) break;
            }
            // Amarelo: Beep rápido intermitente (atenção)
            for (int i = 0; i < 7; i++) { // 3 segundos (7 ciclos de ~428ms)
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(200)); // Beep rápido
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(228)); // Intervalo ajustado para 3s totais
                if (current_mode != MODE_BAIXO_FLUXO) break;
            }
            // Verde: 1 beep curto por segundo (pode atravessar)
            for (int i = 0; i < 15; i++) { // 15 segundos
                gpio_put(BUZZER1, true);
                gpio_put(BUZZER2, true);
                vTaskDelay(pdMS_TO_TICKS(200)); // Beep curto
                gpio_put(BUZZER1, false);
                gpio_put(BUZZER2, false);
                vTaskDelay(pdMS_TO_TICKS(800)); // Intervalo (1s total por ciclo)
                if (current_mode != MODE_BAIXO_FLUXO) break;
            }
        }
    }
}

// Tarefa para o display
void vDisplayTask(void *pvParameters) {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_t ssd;
    // Inicializar o display com dimensões ajustáveis
    ssd1306_init(&ssd, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Pequeno atraso inicial para sincronizar com vMatrixLedTask
    vTaskDelay(pdMS_TO_TICKS(10));

    char state_str[16];
    char time_str[16];

    // Definir a posição da barra
    int bar_y_start = (DISPLAY_HEIGHT == 64) ? 40 : 20; // y=40 para 128x64, y=20 para 128x32
    int bar_y_end = bar_y_start + 10; // Barra de 10 pixels de altura
    int bar_width = 40; // Largura da barra
    int bar_x_start = (DISPLAY_WIDTH - bar_width) / 2; // Centralizado: (128 - 40) / 2 = 44

    while (true) {
        // Limpar o display completamente antes de desenhar
        ssd1306_fill(&ssd, false);

        // Título "Semaf. Intelig." no topo, centralizado
        ssd1306_draw_string(&ssd, "Semaf. Intelig.", 7, 0);

        // Contador de tempo no centro
        int seconds_remaining = time_remaining_ms / 1000;
        if (current_phase == 1) { // Amarelo em qualquer modo
            seconds_remaining = 0; // Zerar o contador para amarelo
        }
        sprintf(time_str, "%d s", seconds_remaining);
        ssd1306_draw_string(&ssd, time_str, 50, 13); // Centralizado verticalmente

        // Modo atual logo abaixo do contador
        if (current_mode == MODE_NORMAL) {
            sprintf(state_str, "Modo: Normal");
        } else if (current_mode == MODE_NOTURNO) {
            sprintf(state_str, "Modo: Noturno");
        } else if (current_mode == MODE_ALTO_FLUXO) {
            sprintf(state_str, "Modo: Alto Fluxo");
        } else if (current_mode == MODE_BAIXO_FLUXO) {
            sprintf(state_str, "Modo: Baixo Fluxo");
        }
        ssd1306_draw_string(&ssd, state_str, 0, 25); // Mantido em y=25

        // Barra de progresso (retângulo preenchido, sem borda, apenas nos modos Normal, Alto Fluxo e Baixo Fluxo)
        if (current_mode != MODE_NOTURNO) {
            int filled_width = 0;
            if (current_phase == 0 || current_phase == 2) { // Verde ou Vermelho
                int total_time_s = 0;
                if (current_mode == MODE_NORMAL) {
                    total_time_s = 20; // 20s para Verde e Vermelho
                } else if (current_mode == MODE_ALTO_FLUXO) {
                    total_time_s = (current_phase == 0) ? 25 : 15; // 25s Verde, 15s Vermelho
                } else if (current_mode == MODE_BAIXO_FLUXO) {
                    total_time_s = (current_phase == 0) ? 15 : 25; // 15s Verde, 25s Vermelho
                }
                filled_width = (time_remaining_ms / 1000) * bar_width / total_time_s; // Proporcional ao tempo restante
            }

            // Desenhar a barra como um retângulo preenchido (sem borda)
            for (int y = bar_y_start; y < bar_y_end; y++) {
                ssd1306_line(&ssd, bar_x_start, y, bar_x_start + filled_width, y, true);
            }
        }

        // Enviar os dados para o display
        ssd1306_send_data(&ssd);
        vTaskDelay(pdMS_TO_TICKS(100)); // Sincroniza com o mesmo intervalo de vMatrixLedTask
    }
}

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events) {
    reset_usb_boot(0, 0);
}

int main() {
    // Para o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();

    // Criação das tarefas
    xTaskCreate(vButtonATask, "Button A Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vMatrixLedTask, "Matrix LED Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vRgbLedTask, "RGB LED Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vDisplayTask, "Display Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}