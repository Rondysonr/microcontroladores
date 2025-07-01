// === BIBLIOTECAS NECESSÁRIAS ===
#include <WiFi.h>            // Gerencia conexão Wi-Fi
#include <PubSubClient.h>    // Cliente MQTT para envio de dados
#include <SPIFFS.h>          // Sistema de arquivos interno do ESP32
#include <WebServer.h>       // Servidor Web embutido
#include <time.h>            // Relógio de tempo real (NTP)
#include <SD.h>              // Acesso ao cartão SD
#include <SPI.h>             // Comunicação SPI para o cartão SD

// === CONFIGURAÇÕES DE REDE E MQTT ===
const char* ssid = "rs";                    // Nome da rede Wi-Fi
const char* password = "12345679";          // Senha da rede Wi-Fi
const char* mqtt_server = "192.168.236.241";// Endereço IP do broker MQTT
const int mqttPort = 1883;                  // Porta padrão MQTT

// === PINOS DO HARDWARE ===
const int LDR_PIN = 34;       // Pino analógico para leitura do sensor de luminosidade
const int SD_CS_PIN = 5;      // Pino CS (Chip Select) do cartão SD

// === OBJETOS GLOBAIS DO SISTEMA ===
WiFiClient espClient;               // Cliente de rede para o MQTT
PubSubClient client(espClient);     // Objeto do cliente MQTT
WebServer server(80);               // Cria o servidor web na porta 80

// === BUFFER PARA DADOS DO SENSOR ===
#define MAX_DATA_POINTS 100         // Número máximo de pontos exibidos no gráfico
struct LDRData {
  String timeStr;                   // Horário da leitura
  float lux;                        // Valor de luminosidade lido
};
LDRData ldrValues[MAX_DATA_POINTS]; // Vetor circular para armazenar leituras recentes
int dataIndex = 0;                  // Índice atual do vetor (para sobrescrever os dados mais antigos)

// === VARIÁVEIS DE CONTROLE DE TEMPO ===
unsigned long ultimaLeitura = 0;           // Marca da última leitura do LDR
const unsigned long intervaloLeitura = 1000; // Intervalo entre leituras (1 segundo)

unsigned long ultimaTentativaWiFi = 0;     // Última tentativa de reconectar Wi-Fi
const unsigned long intervaloWiFi = 10000; // Tenta reconectar a cada 10 segundos

unsigned long ultimaTentativaMQTT = 0;     // Última tentativa de reconectar MQTT
const unsigned long intervaloMQTT = 5000;  // Tenta reconectar a cada 5 segundos

unsigned long ultimaGravacaoSD = 0;        // Última gravação no cartão SD
const unsigned long intervaloGravacaoSD = 5000; // Grava no SD a cada 5 segundos

// === ÚLTIMOS VALORES LIDOS ===
float ultimoValorLido = 0;         // Último valor de luminosidade
String ultimaHoraLida = "";        // Horário da última leitura

bool sdInicializado = false;       // Flag para indicar se o cartão SD está funcionando

// === FUNÇÃO: Conecta ao Wi-Fi ===
void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Espera até 15 segundos para conexão
  unsigned long inicio = millis();
  const unsigned long timeout = 15000;
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < timeout) {
    delay(500);
    Serial.print(".");
  }

  // Verifica se conectou com sucesso
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar ao Wi-Fi.");
  }
}

// === FUNÇÃO: Configura o relógio NTP ===
void configurarRelogioNTP() {
  if (WiFi.status() != WL_CONNECTED) return; // Só tenta se estiver conectado

  configTime(-3 * 3600, 0, "pool.ntp.org"); // Fuso horário UTC-3
  Serial.print("[NTP] Sincronizando");

  // Aguarda sincronização por até 5 segundos
  unsigned long inicio = millis();
  while (time(nullptr) < 100000 && millis() - inicio < 5000) {
    delay(100);
    Serial.print(".");
  }

  Serial.println(time(nullptr) < 100000 ? " OK" : " Falha");
}

// === FUNÇÃO: Retorna a hora formatada ===
String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", timeinfo);
  return String(buffer);
}

// === FUNÇÃO: Salva valor no SPIFFS (memória interna) ===
void saveToSPIFFS(float val, const String& timeStr) {
  File file = SPIFFS.open("/dados.txt", FILE_APPEND);
  if (!file) {
    Serial.println("[SPIFFS] Erro ao abrir dados.txt");
    return;
  }
  file.printf("%s,%.1f\n", timeStr.c_str(), val); // Exemplo: 30/06/2025 14:20:55,520.0
  file.close();
}

// === FUNÇÃO: Salva valor no cartão SD ===
void saveToSD(float val, const String& timeStr) {
  if (!sdInicializado) return;
  File file = SD.open("/log_sd.txt", FILE_APPEND);
  if (!file) {
    Serial.println("[SD] Erro ao abrir log_sd.txt");
    return;
  }
  file.printf("%s,%.1f\n", timeStr.c_str(), val);
  file.close();
  Serial.println("[SD] Gravado no cartão SD");
}

// === FUNÇÃO: Lê o valor do LDR ===
void lerLDR() {
  int raw = analogRead(LDR_PIN); // Lê valor analógico
  float lux = raw; // Pode converter para lux real depois

  String hora = getFormattedTime(); // Pega horário atual

  // Armazena no buffer circular
  ldrValues[dataIndex].lux = lux;
  ldrValues[dataIndex].timeStr = hora;
  dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;

  ultimoValorLido = lux;
  ultimaHoraLida = hora;

  // Salva em SPIFFS
  saveToSPIFFS(lux, hora);

  // Publica no MQTT se estiver conectado
  if (client.connected()) {
    client.publish("sensor/ldr", String(lux, 1).c_str());
    Serial.printf("[MQTT] LDR publicado: %.1f\n", lux);
  }
}

// === FUNÇÃO: Página principal do servidor Web ===
void handleRoot() {
  // HTML completo da interface com gráfico e botões
  String html = R"rawliteral( ... )rawliteral";
  server.send(200, "text/html", html);
}

// === FUNÇÃO: Envia os dados em formato JSON para o gráfico ===
void handleDados() {
  String json = "[";
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    int idx = (dataIndex + i) % MAX_DATA_POINTS;
    json += "{\"time\":\"" + ldrValues[idx].timeStr + "\",\"lux\":" + String(ldrValues[idx].lux, 1) + "}";
    if (i < MAX_DATA_POINTS - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

// === FUNÇÃO: Permite baixar os dados salvos no SPIFFS ===
void handleDownload() {
  if (!SPIFFS.exists("/dados.txt")) {
    server.send(404, "text/plain", "Arquivo não encontrado.");
    return;
  }

  File file = SPIFFS.open("/dados.txt", FILE_READ);
  server.sendHeader("Content-Type", "text/plain");
  server.sendHeader("Content-Disposition", "attachment; filename=dados.txt");
  server.sendHeader("Connection", "close");
  server.streamFile(file, "text/plain");
  file.close();
}

// === FUNÇÃO: Tenta reconectar ao MQTT se necessário ===
void reconnectMQTT() {
  if (client.connected()) return;
  if (millis() - ultimaTentativaMQTT < intervaloMQTT) return;

  ultimaTentativaMQTT = millis();
  Serial.print("[MQTT] Conectando...");
  if (client.connect("ESP32Client")) {
    Serial.println(" OK");
    client.subscribe("sensor/ldr");
  } else {
    Serial.printf(" Falha (rc=%d)\n", client.state());
  }
}

// === FUNÇÃO: Setup (executa uma única vez ao iniciar) ===
void setup() {
  Serial.begin(115200); // Inicializa o monitor serial

  // Inicia SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] Erro ao iniciar");
    return;
  }

  // Cria arquivo inicial se não existir
  if (!SPIFFS.exists("/dados.txt")) {
    File file = SPIFFS.open("/dados.txt", FILE_WRITE);
    if (file) {
      file.println("DataHora,Luminosidade(Lux)");
      file.close();
    }
  }

  // Inicia cartão SD (com pinos definidos)
  SPI.begin(18, 19, 23, SD_CS_PIN);
  if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
    Serial.println("[SD] Cartão SD OK");
    sdInicializado = true;

    if (!SD.exists("/log_sd.txt")) {
      File file = SD.open("/log_sd.txt", FILE_WRITE);
      if (file) {
        file.println("DataHora,Luminosidade(Lux)");
        file.close();
      }
    }
  } else {
    Serial.println("[SD] Falha ao inicializar");
  }

  // Conecta à rede e sincroniza horário
  conectarWiFi();
  configurarRelogioNTP();

  // Configura broker MQTT
  client.setServer(mqtt_server, mqttPort);

  // Rotas do servidor web
  server.on("/", handleRoot);
  server.on("/dados", handleDados);
  server.on("/download", handleDownload);
  server.begin();
  Serial.println("[HTTP] Servidor iniciado");

  // Inicializa vetor de dados
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    ldrValues[i].lux = 0;
    ldrValues[i].timeStr = "";
  }
}

// === FUNÇÃO: Loop principal (executa continuamente) ===
void loop() {
  // Verifica Wi-Fi e reconecta se necessário
  if (WiFi.status() != WL_CONNECTED && millis() - ultimaTentativaWiFi > intervaloWiFi) {
    ultimaTentativaWiFi = millis();
    conectarWiFi();
    configurarRelogioNTP();
  }

  // Reconecta MQTT se caiu
  reconnectMQTT();

  // Atualiza MQTT e servidor Web
  client.loop();
  server.handleClient();

  // Leitura do sensor a cada 1 segundo
  if (millis() - ultimaLeitura >= intervaloLeitura) {
    ultimaLeitura = millis();
    lerLDR();
  }

  // Grava no SD a cada 5 segundos
  if (millis() - ultimaGravacaoSD >= intervaloGravacaoSD) {
    ultimaGravacaoSD = millis();
    saveToSD(ultimoValorLido, ultimaHoraLida);
  }
}
