#include <Arduino.h>
#include "senha.h" // Deve conter #define SSID "nome_da_rede" e #define SENHA "senha_da_rede"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "Adafruit_VL53L0X.h"

// ========= CONFIGURAÇÕES GERAIS / VARIAVEIS =========
const float DISTANCIA_CAIXA = 5.0;       // Apenas objetos a menos de 5cm
const unsigned long tempoEsperaConexao = 10000;

// Configurações MQTT
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_client_id = "senai134_esp02_sensor";
const char *topic_deteccao = "sensor/caixa_detectada";
const char *topic_solicitar_forma = "camera/solicitar_forma";
const char *topic_resposta_forma = "senai/iot/formas";
const char *topic_servo = "servo/comando";

// Objetos
Adafruit_VL53L0X vl53 = Adafruit_VL53L0X();
WiFiClient espClient;
PubSubClient client(espClient);

// Variáveis de controle
bool caixaDetectada = false;
bool caixaDetectadaAnterior = false;
String formaRecebida = "";
bool aguardandoResposta = false;
unsigned long tempoSolicitacao = 0;


// ========= PROTÓTIPOS DE FUNÇÕES ==========
void enviarParaServo(String forma);
void enviarCaixaDetectada(float distancia);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void resetarSistema();


// ========= FUNÇÕES DE REDE ==========

void conectaWiFi() {
  Serial.printf("Conectando ao WiFi: %s\n", SSID);
  WiFi.begin(SSID, SENHA);
  
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < tempoEsperaConexao) {
    Serial.print(".");
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha no WiFi!");
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconectando WiFi...");
    conectaWiFi();
  }
}

void conectaMqtt() {
  while (!client.connected()) {
    Serial.println("Conectando MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("MQTT conectado!");
      // Inscreve no tópico de resposta da câmera
      client.subscribe(topic_resposta_forma);
      Serial.printf("✓ Inscrito em: %s\n", topic_resposta_forma);
    } else {
      Serial.print("Falha MQTT: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String mensagem = "";
  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  
  Serial.printf("MQTT recebido [%s]: %s\n", topic, mensagem.c_str());
  
  if (String(topic) == topic_resposta_forma) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, mensagem);
    
    if (!error && doc.containsKey("forma")) {
      formaRecebida = doc["forma"].as<String>();
    } else {
      formaRecebida = mensagem;
    }
    
    Serial.println("\n=======================================");
    Serial.printf("FORMA IDENTIFICADA: %s\n", formaRecebida.c_str());
    Serial.println("=======================================\n");
    
    enviarParaServo(formaRecebida);
    resetarSistema();
  }
}


// ========= FUNÇÕES DE ENVIO MQTT ==========

void enviarParaServo(String forma) {
  StaticJsonDocument<200> doc;
  doc["forma"] = forma;
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);

  if (client.publish(topic_servo, json.c_str())) {
    Serial.printf("SERVO <- Forma: %s\n", forma.c_str());
  } else {
    Serial.println("Falha ao enviar para servo");
  }
}

void enviarCaixaDetectada(float distancia) {
  StaticJsonDocument<100> doc;
  doc["detectada"] = true; 
  doc["distancia"] = distancia;
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);

  if (client.publish(topic_deteccao, json.c_str())) {
    Serial.println("✓ MQTT → Caixa detectada");
  }
}

void solicitarFormaCamera() {
  StaticJsonDocument<100> doc;
  doc["solicitar"] = true;
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);

  if (client.publish(topic_solicitar_forma, json.c_str())) {
    Serial.println("Solicitando identificacao da camera...");
    aguardandoResposta = true;
    tempoSolicitacao = millis();
  } else {
    Serial.println("Falha ao solicitar forma");
  }
}

// ========= FUNÇÕES DE CONTROLE ==========

void resetarSistema() {
  Serial.println("\nResetando sistema...");
  Serial.println("Aguardando proximo objeto...\n");
  
  aguardandoResposta = false;
  formaRecebida = "";
  caixaDetectada = false;
  caixaDetectadaAnterior = false;
  tempoSolicitacao = 0;
}

// ========= FUNÇÕES DE SENSOR ==========

float lerDistancia() {
  VL53L0X_RangingMeasurementData_t measure;
  vl53.rangingTest(&measure, false);
  
  if (measure.RangeStatus != 4) {
    // Retorna a distância em cm
    return measure.RangeMilliMeter / 10.0;
  }
  // Retorna -1 se houver erro na leitura
  return -1;
}

// ========== SETUP ==========

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n===========================================");
  Serial.println("ESP02 - Sensor de Proximidade + Câmera");
  Serial.println("===========================================\n");

  if (!vl53.begin()) {
    Serial.println("ERRO: VL53L0X falhou!");
    while (1);
  }

  conectaWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  Serial.println("Sistema iniciado!");
  Serial.printf("Aguardando objetos a menos de %.1f cm...\n\n", DISTANCIA_CAIXA);
}

// ========== LOOP ==========

void loop() {
  checkWiFi();

  if (!client.connected()) {
    conectaMqtt();
  }
  client.loop();

  // Lê distância
  float distancia = lerDistancia();
  
  // Verifica se tem objeto a menos de DISTANCIA_CAIXA
  caixaDetectada = (distancia > 0 && distancia < DISTANCIA_CAIXA);

  if (caixaDetectada && !caixaDetectadaAnterior && !aguardandoResposta) {
    Serial.println("\n=======================================");
    Serial.println("OBJETO IDENTIFICADO!");
    Serial.printf("Distancia detectada: %.2f cm\n", distancia);
    Serial.println("=======================================\n");

    enviarCaixaDetectada(distancia);
    solicitarFormaCamera();
  }

  caixaDetectadaAnterior = caixaDetectada;
  delay(100);
}