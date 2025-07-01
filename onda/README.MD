# 🚦 Gerador de Forma de Onda com ESP32 (DAC + PWM Software)

Este projeto implementa, em um ESP32, a geração de uma forma de onda personalizada combinando saída analógica via DAC e saída digital via PWM por software. A forma de onda consiste em uma senoide crescente, seguida de um nível constante, e por fim uma rampa decrescente — ideal para testes em sistemas de controle, drivers de potência, ou simulações de sinais analógicos.

## 📋 Visão Geral

- Frequência da onda: **110 Hz**
- Amostras por ciclo: **100**
- Saída analógica: **GPIO25 via DAC interno**
- Saída digital PWM por software: **GPIO25**
- Forma de onda:
  - 25% do ciclo com senoide crescente
  - 50% com valor constante (255)
  - 25% com rampa decrescente

## 🧠 Funcionamento

1. **Geração da Forma de Onda:**  
   A função `gerar_forma_onda()` preenche um vetor de 100 valores (`forma_onda[]`) com:
   - 1/4 do ciclo com senoide crescente
   - 1/2 do ciclo com valor máximo (255)
   - 1/4 do ciclo com rampa linear decrescente

2. **Saídas:**  
   A cada intervalo (calculado com base na frequência), o sistema:
   - Atualiza o valor do DAC no `GPIO25` (`dac_output_voltage`)
   - Gera um ciclo PWM por software no mesmo pino, com duty cycle proporcional ao valor da amostra

3. **Temporização:**  
   Um objeto `Ticker` dispara a função `atualizar_saidas()` periodicamente para manter a frequência da onda.

## 🔌 Conexões

| Componente        | Pino ESP32 | Descrição                     |
|------------------|------------|-------------------------------|
| DAC interno       | GPIO25     | Saída analógica               |
| PWM por software | GPIO25     | Compartilha o mesmo pino      |

> ⚠️ Certifique-se de não conectar carga diretamente ao GPIO25 sem proteção/resistência.

## ⚙️ Requisitos

- **Placa:** ESP32 (com DAC interno ativo nos GPIO25 ou GPIO26)
- **IDE:** Arduino IDE
- **Bibliotecas:**  
  - `driver/dac.h` (interna do ESP-IDF / Arduino-ESP32)
  - `Ticker.h` (disponível via Gerenciador de Bibliotecas)

## 🚀 Como Usar

1. **Abra o código na Arduino IDE.**
2. **Selecione a placa "ESP32 Dev Module".**
3. **Conecte o ESP32 via USB.**
4. **Compile e faça o upload.**
5. **Conecte um osciloscópio ou multímetro ao GPIO25 para observar a forma de onda.**

## 🛠️ Personalização

- Para alterar a frequência da onda, edite o valor de `frequencia_onda`.
- Para suavizar a senoide ou a rampa, aumente `amostras_por_ciclo`.
- Para trocar o pino PWM (diferente do DAC), edite `pino_pwm`.

## 📈 Aplicações

- Simulação de sinais analógicos
- Testes de filtros, amplificadores ou sistemas de aquisição
- Geração de estímulos para sensores ou atuadores
- Ensino de eletrônica e sinais

## 🧑‍💻 Autor

**Rondyson Raillan Siqueira dos Reis**  
Projeto educacional para experimentos com ESP32.

---

