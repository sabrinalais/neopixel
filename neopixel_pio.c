#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "ws2818b.pio.h" // Biblioteca gerada pelo arquivo .pio durante compilação.

/** Obs: os dados foram obtidos aleatoriamente por meio da função rand(), 
 * pois o sensor estava indisponível para a execução do projeto*/

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;


// Configuração do pino do buzzer
#define BUZZER_PIN 21

// Definição de uma função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

void play_tone(uint pin, uint freq, uint duration) {
  if (freq == 0) {
      sleep_ms(duration);
      return;
  }
  uint slice = pwm_gpio_to_slice_num(pin);
  uint32_t top = clock_get_hz(clk_sys) / freq - 1;
  pwm_set_wrap(slice, top);
  pwm_set_gpio_level(pin, top / 2);
  sleep_ms(duration);
  pwm_set_gpio_level(pin, 0);
  sleep_ms(30);
}

// Definição de uma função para emitir um beep com duração especificada
const uint beep_simples[] = {230};  // Notas: C5, E5, G5, C5, E5, C6
const uint duracao_beep[] = {100};  // Duração de 200 ms para cada nota
void tocar_beepsimples(uint pin) {
    for (int i = 0; i < sizeof(beep_simples) / sizeof(beep_simples[0]); i++) {
        play_tone(pin, beep_simples[i], duracao_beep[i]);
    }
}

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin) {

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0) {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }

  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Limpa buffer de pixels.
  for (uint i = 0; i < LED_COUNT; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

void acender_led(uint led_pin, uint8_t r, uint8_t g, uint8_t b) {
  npSetLED(led_pin, r, g, b);
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
  for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < LED_COUNT; ++i) {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

int main() {

  // Inicializa entradas e saídas.
  stdio_init_all();

  // Inicialização do i2c
  i2c_init(i2c1, ssd1306_i2c_clock * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  // Processo de inicialização completo do OLED SSD1306
  ssd1306_init();

  // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
  struct render_area frame_area = {
      start_column : 0,
      end_column : ssd1306_width - 1,
      start_page : 0,
      end_page : ssd1306_n_pages - 1
  };

  calculate_render_area_buffer_length(&frame_area);

  // zera o display inteiro
  uint8_t ssd[ssd1306_buffer_length];
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, &frame_area);

restart:

srand(time(NULL));

bool alerta_ativo = false;

  // Inicializar o PWM no pino do buzzer
  pwm_init_buzzer(BUZZER_PIN);

  // Inicializa matriz de LEDs NeoPixel.
  npInit(LED_PIN);
  npClear();

  // Definição dos pinos dos LEDs para vermelho e verde
  uint leds_vermelhos[] = {0, 4, 6, 8, 12, 16, 18, 20, 24}; // Array com os índices dos LEDs vermelhos
  uint leds_verdes[] = {3, 5, 7, 11, 19};    // Array com os índices dos LEDs verdes
  uint num_leds_vermelhos = sizeof(leds_vermelhos) / sizeof(leds_vermelhos[0]);
  uint num_leds_verdes = sizeof(leds_verdes) / sizeof(leds_verdes[0]);

  while (true){
    // Dados aleatórios para o sensor
    float temperatura = (float)rand() / RAND_MAX * 25.0 + 15.0;
    float umidade = (float)rand() / RAND_MAX * 50.0 + 30.0;

    char temp_str[20];
    char umid_str[20];
    char alerta_str[20];

    snprintf(temp_str, sizeof(temp_str), "Temp: %.1f C", temperatura);
    snprintf(umid_str, sizeof(umid_str), "Umid: %.1f %%", umidade);


    // Exibe os dados no display
    ssd1306_draw_string(ssd, 0, 0, temp_str);
    ssd1306_draw_string(ssd, 0, 16, umid_str);

    if (temperatura > 30.0){
        snprintf(alerta_str, sizeof(alerta_str), "Temperatura alta!");
        ssd1306_draw_string(ssd, 0, 32, alerta_str);
        alerta_ativo = true;
        // Acende os LEDs vermelhos
        for (int i = 0; i < num_leds_vermelhos; i++) {
          acender_led(leds_vermelhos[i], 255, 0, 0);
        }
        tocar_beepsimples(BUZZER_PIN); // Primeiro beep
        sleep_ms(100); // Pausa entre os beeps
        tocar_beepsimples(BUZZER_PIN); // Segundo beep
    } else if (umidade < 40.0){
        snprintf(alerta_str, sizeof(alerta_str), "Umidade baixa!   ");
        ssd1306_draw_string(ssd, 0, 32, alerta_str);
        alerta_ativo = true;
        // Acende os LEDs vermelhos
        for (int i = 0; i < num_leds_vermelhos; i++) {
          acender_led(leds_vermelhos[i], 255, 0, 0);
        }
        tocar_beepsimples(BUZZER_PIN); // Primeiro beep
        sleep_ms(100); // Pausa entre os beeps
        tocar_beepsimples(BUZZER_PIN); // Segundo beep
    } else if (alerta_ativo){
        snprintf(alerta_str, sizeof(alerta_str), "................."); // Preenche com espaços em branco
        alerta_str[sizeof(alerta_str) - 1] = '\0';   // Garante o terminador nulo
        ssd1306_draw_string(ssd, 0, 32, alerta_str); // Exibe espaços em branco
        alerta_ativo = false; // Desativa o alerta
        // Acende os LEDs verdes
        for (int i = 0; i < num_leds_verdes; i++) {
          acender_led(leds_verdes[i], 0, 255, 0);
        }
        tocar_beepsimples(BUZZER_PIN); // Um beep longo
    } else {
      // Acende os LEDs verdes
      for (int i = 0; i < num_leds_verdes; i++) {
        acender_led(leds_verdes[i], 0, 255, 0);
      }
      tocar_beepsimples(BUZZER_PIN); // Um beep longo
    }


    render_on_display(ssd, &frame_area);

    npWrite();
    npClear();

    sleep_ms(2000);
  };
  return 0;
}
