#include <Arduino.h>
#include <driver/dac.h>
#include <Ticker.h>

// Frequência da forma de onda principal (em Hz)
const int frequencia_onda = 110;

// Número de amostras por ciclo da onda
const int amostras_por_ciclo = 100;

// Período total da onda (em milissegundos)
const float periodo_ms = 1000.0 / frequencia_onda;

// Vetor que armazena os valores da onda personalizada (0 a 255)
uint8_t forma_onda[amostras_por_ciclo];

// Índice da amostra atual
volatile int indice_onda = 0;

// Temporizador para atualizar a forma de onda
Ticker temporizador;

// Pino do PWM por software
const int pino_pwm = 25;

// Período do PWM por software (calculado em tempo de execução)
float periodo_pwm_us = 0;  // será calculado no setup()

// Função que gera a forma de onda:
// 1/4 ciclo com senoide crescente
// 1/2 ciclo com valor constante (255)
// 1/4 ciclo com rampa decrescente
void gerar_forma_onda() {
  int i = 0;

  // 1/4 ciclo: senoide crescente (0 → 255)
  int n_senoide = amostras_por_ciclo / 4;
  for (; i < n_senoide; i++) {
    float angulo = ((float)i / (n_senoide - 1)) * (PI / 2);
    forma_onda[i] = (uint8_t)(255.0 * sin(angulo));
  }

  // 1/2 ciclo: valor constante (255)
  int n_constante = amostras_por_ciclo / 2;
  for (int j = 0; j < n_constante; j++, i++) {
    forma_onda[i] = 255;
  }

  // 1/4 ciclo: rampa decrescente (255 → 0)
  int n_rampa = amostras_por_ciclo - i;
  for (int j = 0; j < n_rampa; j++, i++) {
    forma_onda[i] = (uint8_t)(255.0 * (1.0 - (float)j / (n_rampa - 1)));
  }
}

// Função que gera um ciclo de PWM por software com ciclo de trabalho variável
void pwm_software(uint8_t duty) {
  int tempo_alto = periodo_pwm_us * duty / 255.0;
  int tempo_baixo = periodo_pwm_us - tempo_alto;

  digitalWrite(pino_pwm, HIGH);
  if (tempo_alto > 0) delayMicroseconds(tempo_alto);

  digitalWrite(pino_pwm, LOW);
  if (tempo_baixo > 0) delayMicroseconds(tempo_baixo);
}

// Atualiza a saída do DAC e gera PWM com base na amostra atual
void atualizar_saidas() {
  uint8_t valor = forma_onda[indice_onda];

  // Saída analógica via DAC (GPIO25)
  dac_output_voltage(DAC_CHANNEL_1, valor);

  // Gera PWM por software
  pwm_software(valor);

  // Próxima amostra
  indice_onda = (indice_onda + 1) % amostras_por_ciclo;
}

void setup() {
  Serial.begin(115200);

  // Configura o pino do PWM como saída
  pinMode(pino_pwm, OUTPUT);

  // Habilita o DAC (GPIO25)
  dac_output_enable(DAC_CHANNEL_1);

  // Gera a forma de onda personalizada
  gerar_forma_onda();

  // Calcula o intervalo entre amostras
  float intervalo_us = (periodo_ms * 1000.0) / amostras_por_ciclo;

  // Calcula o período do PWM (1 ciclo da onda / 5000)
  periodo_pwm_us = (periodo_ms * 1000.0) / 5000.0;

  // Inicia o temporizador para atualizar as saídas
  temporizador.attach_us(intervalo_us, atualizar_saidas);
}

void loop() {
  // Nada aqui — execução por interrupções via Ticker
}
