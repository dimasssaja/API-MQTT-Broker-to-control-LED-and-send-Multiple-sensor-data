#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHT_PIN 18
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

#define MQ5_PIN 34
#define RAIN_PIN 35
#define BUZZER_PIN 25
#define TONE_FREQUENCY 2000

#define LED_RED 32
#define LED_GREEN 12
#define LED_BLUE 13

const char* ssid = "Proton";
const char* password = "Sunda123";
const char* mqttServer = "efeeee8f.ala.us-east-1.emqxsl.com";
const char* sensorTopic = "leds";
const char* ledTopic = "ledsred";
const char* mqtt_username = "dimas";
const char* mqtt_password = "dimas";
int port = 8883;
unsigned long lastTime = 0;
unsigned long intervalTime = 5000;

String baseUrl = "https://abdimasiot.web.id/api/";
String temperatureEndpoint = baseUrl + "leds";

static const char* root_ca PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure espClient;
PubSubClient client(espClient);

struct LEDData {
  String name;
  bool status;
  String user_id;
};

LEDData ledData;

void setup() {
  Serial.begin(115200);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Sensor Gas, DHT11, Rain");
  display.display();
  delay(2000);
  display.clearDisplay();

  pinMode(RAIN_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  Serial.println("\nConnected to WiFi");

  espClient.setCACert(root_ca);
  client.setServer(mqttServer, port);
  client.setCallback(callback);
  while (!client.connected()) {
    mqttInit();
  }
}

void loop() {
  client.loop();

  if (!client.connected()) {
    mqttReconnect();
  }

  // Ambil status LED dan publikasikan ke MQTT setiap 5 detik
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= intervalTime) {
    lastTime = currentTime;
    publishLEDStatus();
  }
}

void mqttInit() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT server");
      client.subscribe(ledTopic);
    } else {
      Serial.print("Failed to connect, status code: ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void mqttReconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT reconnection...");
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("Reconnected to MQTT server");
      client.subscribe(ledTopic);
    } else {
      Serial.print("Failed to reconnect, status code: ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  JSONVar jsonData = JSON.parse(message);
  if (JSON.typeof(jsonData) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }
  int ledStatus = jsonData["status"];

  if (String(topic) == ledTopic) {
    if (ledStatus == 1) {
      digitalWrite(LED_RED, HIGH);
    } else {
      digitalWrite(LED_RED, LOW);
    }
  }
}

void publishLEDStatus() {
  // Ambil status LED
  int status = digitalRead(LED_RED);

  // Kirim status LED ke MQTT
  JSONVar ledData;
  ledData["status"] = status;
  String payload = JSON.stringify(ledData);
  client.publish(ledTopic, payload.c_str());
  Serial.println("Published LED status to MQTT");
}
