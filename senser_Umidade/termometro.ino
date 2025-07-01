#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Configurações da rede Wi-Fi
const char* ssid = "rs";
const char* password = "12345679";
const char* mqtt_server = "192.168.236.241";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);

#define MAX_POINTS 50
float temperaturas[MAX_POINTS] = {0};
float umidades[MAX_POINTS] = {0};
int dataIndex = 0;

// Página web com gráfico melhorado (Temperatura e Umidade)
void handleRoot() {
  String html = R"rawliteral(
    <html>
    <head>
      <title>Gráfico de Temperatura e Umidade</title>
      <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
      <style>
        body { font-family: Arial; background: #f5f5f5; }
        .container { background: #fff; margin: 30px auto; padding: 20px; border-radius: 10px; width: 600px; box-shadow: 0 0 10px #ccc;}
        h2 { text-align: center; }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>Temperatura e Umidade em tempo real</h2>
        <canvas id='grafico' width='550' height='300'></canvas>
      </div>
      <script>
        let ctx = document.getElementById('grafico').getContext('2d');
        let chart = new Chart(ctx, {
          type: 'line',
          data: {
            labels: Array.from({length: 50}, (_,i)=>i+1),
            datasets: [
              {
                label: 'Temperatura (°C)',
                data: [],
                borderColor: 'red',
                backgroundColor: 'rgba(255,0,0,0.07)',
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 2
              },
              {
                label: 'Umidade (%)',
                data: [],
                borderColor: 'blue',
                backgroundColor: 'rgba(0,0,255,0.07)',
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 2
              }
            ]
          },
          options: {
            animation: false,
            responsive: false,
            scales: {
              y: {
                beginAtZero: true,
                title: { display: true, text: 'Valor' }
              },
              x: {
                title: { display: true, text: 'Amostra' }
              }
            },
            plugins: {
              legend: {
                display: true,
                position: 'top'
              },
              title: {
                display: true,
                text: 'Últimas 50 Amostras'
              }
            }
          }
        });

        function atualizarGrafico() {
          fetch('/dados')
            .then(res => res.json())
            .then(dados => {
              chart.data.labels = dados.labels;
              chart.data.datasets[0].data = dados.temperaturas;
              chart.data.datasets[1].data = dados.umidades;
              chart.update();
            });
        }
        setInterval(atualizarGrafico, 2000);
        atualizarGrafico();
      </script>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// Endpoint para enviar os dados de temperatura e umidade (JSON)
void handleDados() {
  String json = "{";
  json += "\"labels\":[";
  for (int i = 0; i < MAX_POINTS; i++) {
    json += String(i+1);
    if (i < MAX_POINTS - 1) json += ",";
  }
  json += "],\"temperaturas\":[";
  for (int i = 0; i < MAX_POINTS; i++) {
    json += String(temperaturas[(dataIndex + i) % MAX_POINTS], 1);
    if (i < MAX_POINTS - 1) json += ",";
  }
  json += "],\"umidades\":[";
  for (int i = 0; i < MAX_POINTS; i++) {
    json += String(umidades[(dataIndex + i) % MAX_POINTS], 1);
    if (i < MAX_POINTS - 1) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// Callback MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.print("Recebido do tópico ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, message);

  if (err) {
    Serial.print("Erro ao interpretar JSON: ");
    Serial.println(err.c_str());
    return;
  }

  // Correção aqui: usando as<float>() para conversão
  float temp = doc["temperatura"].as<float>();
  float umi = doc["umidade"].as<float>();

  temperaturas[dataIndex] = temp;
  umidades[dataIndex] = umi;
  dataIndex = (dataIndex + 1) % MAX_POINTS;

  Serial.print("Temperatura registrada: ");
  Serial.println(temp);
  Serial.print("Umidade registrada: ");
  Serial.println(umi);
}

// Reconecta ao MQTT se necessário
void reconnect() {
  while (!client.connected()) {
    Serial.print("Testando ping ao broker MQTT: ");
    Serial.println(mqtt_server);
    Serial.print("Conectando ao broker MQTT...");
    if (client.connect("ESP32_RECEPTOR")) {
      Serial.println("Conectado ao MQTT.");
      client.subscribe("sensor/dht11");
    } else {
      Serial.print("Erro MQTT: ");
      Serial.print(client.state());
      Serial.println(". Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

// Conexão Wi-Fi
void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long inicio = millis();
  const unsigned long timeout = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < timeout) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar ao Wi-Fi.");
  }
}

// Setup do ESP32
void setup() {
  Serial.begin(115200);
  conectarWiFi();

  client.setServer(mqtt_server, mqttPort);
  client.setCallback(mqttCallback);

  server.on("/", handleRoot);
  server.on("/dados", handleDados);
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

// Loop principal
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  server.handleClient();
}