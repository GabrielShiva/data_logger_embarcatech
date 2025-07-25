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

// Variáveis de estado do sistema
static volatile bool data_logger_state = false;
static volatile bool is_mount_runned = false;

// Informações do arquivo gerado
static char file_name[20] = "adc_data.csv";

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

// Definição do protótipo das funções que serão criadas
void gpio_irq_handler(uint gpio, uint32_t events);
void get_sensor_data();

int main() {
    stdio_init_all();

    const bool color = true;
    char buffer[100];

    // Iniciallização dos botões
    btns_init();

    gpio_set_irq_enabled_with_callback(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BTN_A_PIN, GPIO_IRQ_EDGE_RISE, true);

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
    ssd1306_draw_string(&ssd, buffer, 5, 30);
    sprintf(buffer, "INICIALIZADO");
    ssd1306_draw_string(&ssd, buffer, 5, 40);
    ssd1306_send_data(&ssd);

    sleep_ms(2500);

    ssd1306_fill(&ssd, !color);
    ssd1306_send_data(&ssd);

    while (true) {
        // Realizar a montagem do cartão SD (pressionar BTN B)
        if (is_mount_runned) {
            bool success = run_mount();

            if (success) {

            } else {
                is_mount_runned = false;
            }
        }

        // Realizar a desmontagem do cartão SD (pressionar BTN B)
        if (!is_mount_runned) {
            bool success = run_unmount();
        }

        // // Realiza a leitura dos sensores integrados no MPU6050
        // mpu6050_read_raw(I2C0_PORT, accel, gyro, &temp);

        // // Conversão para float dos valores lidos pelo giroscópio
        // sensor_data.gyro_x = (float)gyro[0];
        // sensor_data.gyro_y = (float)gyro[1];
        // sensor_data.gyro_z = (float)gyro[2];

        // // Conversão para float dos valores lidos pelo acelerômetro e adequação à escala (g=9.81 m/s^2)
        // sensor_data.accel_x = accel[0] / 16384.0f;
        // sensor_data.accel_y = accel[1] / 16384.0f;
        // sensor_data.accel_z = accel[2] / 16384.0f;

        // printf("----\n");
        // printf("ACCEL X: %.2f, Y: %.2f, Z: %.2f \n", sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z);
        // printf("GYRO X: %.2f, Y: %.2f, Z: %.2f \n", sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z);

        sleep_ms(200);
    }
}

// Função responsável por realizar o tratamento das interrupções geradas pelos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    // Muda a página exibida no display
    if (gpio == BTN_A_PIN && (current_time - last_btn_a_press > DEBOUNCE_TIME)) {

    // Monta e desmonta o cartão sd
    } else if (gpio == BTN_B_PIN && (current_time - last_btn_b_press > DEBOUNCE_TIME)) {
        last_btn_b_press = current_time;

        is_mount_runned = !is_mount_runned;

        // reset_usb_boot(0, 0);
    }
}

// Captura os dados do sensor MPU6050
void get_sensor_data() {

}
