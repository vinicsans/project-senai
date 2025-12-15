#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h> // Incluindo a biblioteca JSON

// --- Protótipos ---
void setup_wifi();
void callback(char *topic, byte *payload, unsigned int length);
void reconnect();

// --- Configurações ---
const char *ssid = "SENAI IoT";
const char *password = "Senai@IoT";
const char *mqtt_server = "broker.hivemq.com";
const char *topic = "servo/comando";

// --- Ajustes do Servo ---
Servo meuServo;
const int servoPin = 13;
int anguloVertical = 90;

WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  meuServo.setPeriodHertz(50);
  meuServo.attach(servoPin, 500, 2400);

  meuServo.write(anguloVertical);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
}

void setup_wifi()
{
  delay(10);
  Serial.print("Conectando em ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
}

// --- CALLBACK COM TRATAMENTO JSON ---
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.println("-----------------------");
  Serial.print("Mensagem recebida: ");

  // 1. Criar um documento JSON (tamanho de 256 bytes é suficiente para mensagens curtas)
  JsonDocument doc;

  // 2. Deserializar a mensagem recebida
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error)
  {
    Serial.print("Falha no JSON: ");
    Serial.println(error.f_str());
    return;
  }

  // 3. Extrair o valor da chave "forma"
  // Se a chave não existir, o valor será "null"
  const char *forma = doc["forma"];

  if (forma == NULL)
  {
    Serial.println("Chave 'forma' não encontrada no JSON.");
    return;
  }

  Serial.print("Forma identificada: ");
  Serial.println(forma);

  // 4. Lógica de movimento baseada na string extraída
  if (strcmp(forma, "Triangulo") == 0)
  {
    Serial.println("Ação: Triângulo -> Movendo para 0°");
    meuServo.write(0);
    delay(1000);
    meuServo.write(anguloVertical);
  }
  else if (strcmp(forma, "Circulo") == 0 || strcmp(forma, "Quadrado") == 0)
  {
    Serial.println("Ação: Círculo/Quadrado -> Movendo para 180°");
    meuServo.write(180);
    delay(1000);
    meuServo.write(anguloVertical);
  }
  else
  {
    Serial.println("Comando desconhecido dentro do campo 'forma'.");
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Tentando MQTT...");
    if (client.connect("ESP32_Logic_Unique_99"))
    { // ID alterado para evitar conflito
      Serial.println("Conectado!");
      client.subscribe(topic);
    }
    else
    {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}
