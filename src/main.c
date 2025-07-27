#include <stdio.h>
#include <time.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/rtc.h"

// Inclusão dos módulos
#include "inc/button/button.h"
#include "inc/buzzer/buzzer.h"
#include "inc/display/ssd1306.h"
#include "inc/i2c_protocol/i2c_protocol.h"
#include "inc/led_rgb/led.h"
#include "inc/sensors/mpu6050.h"
#include "inc/sd_card_func/sd_card_func.h"

// Definição de variáveis e macros importantes para o debounce dos botões
#define DEBOUNCE_TIME 260

static volatile uint32_t last_btn_a_press = 0;
static volatile uint32_t last_btn_b_press = 0;
static volatile uint32_t last_btn_sw_press = 0;

// Variáveis de estado do sistema
static volatile bool data_logger_state = false;
static volatile bool is_mount_runned = false;

// Informações do arquivo gerado
static char file_name[20] = "adc_data.csv";

static volatile uint counter = 0;
static volatile uint file_counter = 0;

typedef enum {
    DISPLAY_MENU,
    DISPLAY_ACTION_MESSAGE
} display_state_t;

static display_state_t display_state = DISPLAY_MENU;

typedef struct {
    char line1[22];
    char line2[22];
    char line3[22];
    uint32_t duration_ms;
    absolute_time_t start_time;
    bool active;
} action_message_t;

static action_message_t current_message = {0};

typedef enum {
    MENU_MAIN = 0,
    MENU_SAMPLING = 1,
    MENU_MAX
} menu_page_t;

static volatile menu_page_t menu_page = MENU_MAIN;

absolute_time_t led_time = 0;

typedef enum {
    SAMPLING_IDLE,
    SAMPLING_RUNNING,
    SAMPLING_STOPPING
} sampling_state_t;

volatile sampling_state_t sampling_state = SAMPLING_IDLE;

// Definição de variáveis para os valores lidos pelos sensores
int16_t accel[3];
int16_t gyro[3];
int16_t temp;

typedef struct sensor_data {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} sensor_data_t;

sensor_data_t sensor_data;

ssd1306_t ssd;

static bool color = true;
char buffer[100];

// Definição do protótipo das funções que serão criadas
void gpio_irq_handler(uint gpio, uint32_t events);
void show_main_menu();
void show_action_message(const char* l1, const char* l2, const char* l3, uint32_t duration);
void show_sampling_menu();
void get_sensor_data();

int main() {
    stdio_init_all();

    // Iniciallização dos botões
    btns_init();

    gpio_set_irq_enabled_with_callback(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BTN_A_PIN, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_SW_PIN, GPIO_IRQ_EDGE_RISE, true);

    // Inicialização do LED RGB
    leds_init();

    // Inicialização dos buzzers
    buzzer_setup(BUZZER_RIGHT_PIN);

    // Inicialização do barramento I2C sensores AHT20 e BMP280
    i2c_setup(I2C0_SDA, I2C0_SCL);

    // Inicializa o MPU6050
    printf("Inicializando o MPU6050...\n");
    mpu6050_reset(I2C0_PORT);

    //Inicialização do barramento I2C para o display
    i2c_setup(I2C1_SDA, I2C1_SCL);

    printf("Inicializando o display...\n");
    // Inicializa o display
    ssd1306_setup(&ssd, WIDTH, HEIGHT, false, DISP_ADDR, I2C1_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    printf("Tudo pronto...\n");

    ssd1306_fill(&ssd, !color);
    ssd1306_send_data(&ssd);

    sprintf(buffer, "DATA LOGGER");
    ssd1306_draw_string(&ssd, buffer, 5, 20);
    sprintf(buffer, "INICIALIZADO");
    ssd1306_draw_string(&ssd, buffer, 5, 30);
    ssd1306_send_data(&ssd);

    sleep_ms(2500);

    ssd1306_fill(&ssd, !color);
    ssd1306_send_data(&ssd);

    FIL file;
    uint file_open_counter = 0;

    while (true) {
        // Exibição do menu principal
        if (display_state == DISPLAY_MENU) {
            switch (menu_page) {
                case MENU_MAIN:
                    show_main_menu();
                    break;

                case MENU_SAMPLING:
                    show_sampling_menu();
                    break;

                default:
                    break;
            }
        }

        if (absolute_time_diff_us(led_time, get_absolute_time()) / 1000 > 600 && sampling_state == SAMPLING_RUNNING) {
            led_time = get_absolute_time();
            leds_turnoff();
            gpio_put(BLUE_LED_PIN, 1);
        } else {
            leds_turnoff();
        }

        // Realizar a montagem do cartão SD (pressionar BTN B)
        if (is_mount_runned && counter == 0) {
            printf("mount\n");

            leds_turnoff();
            gpio_put(BLUE_LED_PIN, 1);

            show_action_message("Montando o", "Cartao SD", "", 2500);

            bool success = run_mount();

            if (success) {
                leds_turnoff();
                gpio_put(GREEN_LED_PIN, 1);
                show_action_message("Cartao SD", "Montado com", "Sucesso", 2500);
                is_mount_runned = true;
                counter = 1;
            } else {
                leds_turnoff();
                gpio_put(RED_LED_PIN, 1);
                show_action_message("Falha ao", "Montar o", "Cartao SD", 2500);
                is_mount_runned = false;
                counter = 0;
            }

            leds_turnoff();
        }

        // Realizar a desmontagem do cartão SD (pressionar BTN B)
        if (!is_mount_runned && counter == 1) {
            leds_turnoff();
            gpio_put(BLUE_LED_PIN, 1);

            show_action_message("Desmontando o", "Cartao SD", "", 2500);

            bool success = run_unmount();

            if (success) {
                leds_turnoff();
                gpio_put(GREEN_LED_PIN, 1);
                show_action_message("Cartao SD", "Desmontado", "com Sucesso", 2500);
                is_mount_runned = false;
                counter = 0;
            } else {
                leds_turnoff();
                gpio_put(RED_LED_PIN, 1);
                show_action_message("Falha ao", "Desmontar o", "Cartao SD", 2500);
                is_mount_runned = true;
                counter = 1;
            }

            leds_turnoff();
        }

        // Realiza a amostragem e salva no arquivo
        if (sampling_state == SAMPLING_RUNNING) {
            FRESULT res = FR_OK;

            // Ativa o led azul
            leds_turnoff();
            gpio_put(BLUE_LED_PIN, 1);

            if (file_open_counter == 0) {
                res = f_open(&file, file_name, FA_WRITE | FA_CREATE_ALWAYS);
                file_open_counter++;
            }

            if (res != FR_OK) {
                leds_turnoff();
                gpio_put(RED_LED_PIN, 1);

                show_action_message("Erro ao", "Iniciar", "Coleta", 2000);
                show_action_message("Realize a", "Montagem", "do Cartao SD", 1500);

                file_open_counter = 0;
                sampling_state = SAMPLING_IDLE;

                gpio_put(RED_LED_PIN, 0);
            } else {
                UINT bw;

                if (file_counter == 0) {
                    sprintf(buffer, "numero_amostra,accel_x,accel_y,accel_z,giro_x,giro_y,giro_z\n");
                    res = f_write(&file, buffer, strlen(buffer), &bw);
                    printf("cabecalho\n");
                } else {
                    get_sensor_data();

                    sprintf(buffer, "%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                        file_counter,
                        sensor_data.accel_x,sensor_data.accel_y,sensor_data.accel_z,
                        sensor_data.gyro_x,sensor_data.gyro_y,sensor_data.gyro_z
                    );
                    res = f_write(&file, buffer, strlen(buffer), &bw);
                }

                file_counter++;
            }
        }

        // Fecha o arquivo
        if (sampling_state == SAMPLING_STOPPING) {
            f_close(&file);

            leds_turnoff();
            gpio_put(BLUE_LED_PIN, 0);
            gpio_put(GREEN_LED_PIN, 1);

            show_action_message("Coleta de", "Dados", "Encerrada", 2500);
            // Reset de variáveis
            file_counter = 0;
            file_open_counter = 0;

            leds_turnoff();

            // Volta ao estado inicial
            sampling_state = SAMPLING_IDLE;
        }

        sleep_ms(100);
    }

    return 0;
}

void show_action_message(const char* l1, const char* l2, const char* l3, uint32_t duration) {
    ssd1306_fill(&ssd, !color);
    ssd1306_send_data(&ssd);

    ssd1306_draw_string(&ssd, l1, 5, 20);
    ssd1306_draw_string(&ssd, l2, 5, 30);
    ssd1306_draw_string(&ssd, l3, 5, 40);
    ssd1306_send_data(&ssd);

    sleep_ms(duration);
}

void show_main_menu() {
    ssd1306_fill(&ssd, !color);
    ssd1306_send_data(&ssd);

    if (sampling_state == SAMPLING_IDLE) {
        sprintf(buffer, "A - INICIAR");
    } else if (sampling_state == SAMPLING_RUNNING) {
        sprintf(buffer, "A - PARAR");
    } else {
        sprintf(buffer, "ENCERRANDO...");
    }
    ssd1306_draw_string(&ssd, buffer, 5, 20);

    if (!is_mount_runned && counter == 0) {
        sprintf(buffer, "B - MONTAR");
    } else {
        sprintf(buffer, "B - DESMONTAR");
    }
    ssd1306_draw_string(&ssd, buffer, 5, 30);

    ssd1306_send_data(&ssd);
}

void show_sampling_menu() {
    ssd1306_fill(&ssd, !color);
    ssd1306_send_data(&ssd);

    if (sampling_state == SAMPLING_RUNNING) {
        sprintf(buffer, "QTND: %d", file_counter);
        ssd1306_draw_string(&ssd, buffer, 5, 30);
    } else {
        sprintf(buffer, "Amostragem");
        ssd1306_draw_string(&ssd, buffer, 5, 20);
        sprintf(buffer, "De Dados Nao");
        ssd1306_draw_string(&ssd, buffer, 5, 30);
        sprintf(buffer, "Foi Iniciada");
        ssd1306_draw_string(&ssd, buffer, 5, 40);
    }

    ssd1306_send_data(&ssd);
}

// Função responsável por realizar o tratamento das interrupções geradas pelos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (gpio == BTN_A_PIN && (current_time - last_btn_a_press > DEBOUNCE_TIME)) { // inicia e para a amostragem do sensor MPU6050
        last_btn_a_press = current_time;

        if (sampling_state == SAMPLING_IDLE) {
            sampling_state = SAMPLING_RUNNING;
        } else if (sampling_state == SAMPLING_RUNNING) {
            sampling_state = SAMPLING_STOPPING;
        }
    } else if (gpio == BTN_B_PIN && (current_time - last_btn_b_press > DEBOUNCE_TIME)) { // Monta e desmonta o cartão sd
        last_btn_b_press = current_time;

        if (file_counter == 0) {
            is_mount_runned = !is_mount_runned;
        }
    } else if (gpio == BTN_SW_PIN && (current_time - last_btn_sw_press > DEBOUNCE_TIME)) { // Muda de página
        last_btn_sw_press = current_time;

        menu_page = (menu_page + 1) % MENU_MAX;
    }
}

// Captura os dados do sensor MPU6050
void get_sensor_data() {
    // Realiza a leitura dos sensores integrados no MPU6050
    mpu6050_read_raw(I2C0_PORT, accel, gyro, &temp);

    // Conversão para float dos valores lidos pelo giroscópio
    sensor_data.gyro_x = (float)gyro[0];
    sensor_data.gyro_y = (float)gyro[1];
    sensor_data.gyro_z = (float)gyro[2];

    // Conversão para float dos valores lidos pelo acelerômetro e adequação à escala (g=9.81 m/s^2)
    sensor_data.accel_x = accel[0] / 16384.0f;
    sensor_data.accel_y = accel[1] / 16384.0f;
    sensor_data.accel_z = accel[2] / 16384.0f;

    printf("----\n");
    printf("ACCEL X: %.2f, Y: %.2f, Z: %.2f \n", sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z);
    printf("GYRO X: %.2f, Y: %.2f, Z: %.2f \n", sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z);
}
