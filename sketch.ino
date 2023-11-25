#include <WiFi.h>
#include <PubSubClient.h>

// Definições de constantes
#define PULSE_PER_BEAT    1
#define INTERRUPT_PIN     5
#define SAMPLING_INTERVAL 1000
#define TOPICO_SUBSCRIBE  "/TEF/insu602/cmd"
#define TOPICO_PUBLISH    "/TEF/insu602/attrs"
#define TOPICO_PUBLISH_2  "/TEF/insu602/attrs/c"
#define ID_MQTT  "fiware_insu602"
#define BROKER_MQTT "46.17.108.113"
#define BROKER_PORT 1883

// Variáveis globais
volatile uint16_t pulse = 0;
uint16_t count = 0;
float heartRate = 0.0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";
WiFiClient espClient;
PubSubClient MQTT(espClient);
int D4 = 2;
char EstadoSaida = '0';

// Protótipos de funções
void initWiFi();
void initMQTT();
void reconectWiFi();
void reconnectMQTT();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void verificaConexoesWiFIEMQTT();
void initOutput();
void heartRateCalculation();
void updateMQTTState();

// Interrupção para cálculo da frequência cardíaca
void IRAM_ATTR HeartRateInterrupt() {
  portENTER_CRITICAL_ISR(&mux);
  pulse++;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup() {
  Serial.begin(115200);

  pinMode(INTERRUPT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), HeartRateInterrupt, RISING);

  initOutput();
  initWiFi();
  initMQTT();
  delay(5000);
  MQTT.publish(TOPICO_PUBLISH, "s|on");
}

void loop() {
  verificaConexoesWiFIEMQTT();
  heartRateCalculation();
  updateMQTTState();
}

void initWiFi() {
  delay(10);
  Serial.println("------Conexao WI-FI------");
  Serial.print("Conectando-se na rede: ");
  Serial.println(SSID);
  Serial.println("Aguarde");

  reconectWiFi();
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    char c = (char)payload[i];
    msg += c;
  }
  Serial.print("- Mensagem recebida: ");
  Serial.println(msg);

  if (msg.equals("lamp001@on|")) {
    digitalWrite(D4, HIGH);
    EstadoSaida = '1';
  } else if (msg.equals("lamp001@off|")) {
    digitalWrite(D4, LOW);
    EstadoSaida = '0';
  }
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("* Tentando se conectar ao Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado com sucesso ao broker MQTT!");
      MQTT.subscribe(TOPICO_SUBSCRIBE);
    } else {
      Serial.println("Falha ao reconectar no broker.");
      Serial.println("Havera nova tentatica de conexao em 2s");
      delay(2000);
    }
  }
}

void reconectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado com sucesso na rede ");
  Serial.print(SSID);
  Serial.println("IP obtido: ");
  Serial.println(WiFi.localIP());
}

void verificaConexoesWiFIEMQTT() {
  if (!MQTT.connected()) {
    reconnectMQTT();
  }
  reconectWiFi();
}

void initOutput() {
  pinMode(D4, OUTPUT);
  digitalWrite(D4, HIGH);

  boolean toggle = false;
  for (int i = 0; i <= 10; i++) {
    toggle = !toggle;
    digitalWrite(D4, toggle);
    delay(200);
  }
}

void heartRateCalculation() {
  static unsigned long startTime = millis();
  if (millis() - startTime >= SAMPLING_INTERVAL) {
    startTime = millis();
    portENTER_CRITICAL(&mux);
    count = pulse;
    pulse = 0;
    portEXIT_CRITICAL(&mux);

    heartRate = map(count, 0, 220, 0, 220);
    Serial.println("Heart Rate: " + String(heartRate, 2) + " BPM");

    char msgBuffer[50];
    snprintf(msgBuffer, 50, "%f", heartRate);
    MQTT.publish(TOPICO_PUBLISH_2, msgBuffer);
  }
  MQTT.loop();
}

void updateMQTTState() {
  static char lastEstadoSaida = '2'; // Inicialmente um valor diferente de '0' ou '1'
  if (EstadoSaida != lastEstadoSaida) {
    lastEstadoSaida = EstadoSaida;
    MQTT.publish(TOPICO_PUBLISH, EstadoSaida == '1' ? "1" : "0");
  }
}
