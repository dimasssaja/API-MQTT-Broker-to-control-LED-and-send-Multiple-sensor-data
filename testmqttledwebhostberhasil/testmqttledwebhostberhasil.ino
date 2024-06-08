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

const char* ssid = "your wifi name";
const char* password = "wifipassword";
const char* mqttServer = "youremqxserver"; 
const char* topic = "yourmqtttopic"; 
const char* mqtt_username = "yourmqttusername";
const char* mqtt_password = "yourmqttpassword";
int port = 8883;
unsigned long lastTime = 0;
unsigned long intervalTime = 5000;

String baseUrl = "https://abdimasiot.web.id/api/";
String temperatureEndpoint = baseUrl + "sensor";

static const char* root_ca PROGMEM = ;

WiFiClientSecure espClient;
PubSubClient client(espClient);

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
    mqttInit();
  }

  sendData();

  delay(2000);
}

void insertData(float temperature, float humidity, float gas, int rain) {
  HTTPClient http;
  JSONVar dataObject;
  dataObject["temperature"] = temperature;
  dataObject["humidity"] = humidity;
  dataObject["gas_level"] = gas;
  dataObject["rain_detected"] = rain;
    dataObject["device_id"] = (int) 1;

  String sData = JSON.stringify(dataObject);

  Serial.println("Sending data to API:");
  Serial.println(sData);

  http.begin(temperatureEndpoint);
  http.addHeader("Content-Type", "application/json");
  int statusCode = http.POST(sData);

  if (statusCode > 0) {
    Serial.println("API response: " + String(statusCode));
    String payload = http.getString();
    client.publish(topic, payload.c_str());
  } else {
    Serial.println("Error sending data: " + String(statusCode));
  }

  http.end();
}

void dhtRead(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  }
}

void readMQ5(float &gas) {
  int sensorValue = analogRead(MQ5_PIN);
  float voltage = sensorValue * (5.0 / 1023.0) * 100;
  gas = (voltage - 40) / 460 * 300;
}

int readRainSensor() {
  return digitalRead(RAIN_PIN);
}

bool ledStatus = false;
void controlLED(bool status) {
  digitalWrite(LED_RED, status ? HIGH : LOW);
  ledStatus = status;
}

void sendData() {
  if ((millis() - lastTime) > intervalTime) {
    float temperature = 0;
    float humidity = 0;
    float gas = 0;
    int rain = 0;

    dhtRead(temperature, humidity);
    readMQ5(gas);
    rain = readRainSensor();

    if (gas >= 100 || rain == 0) {
      digitalWrite(BUZZER_PIN, HIGH);
      tone(BUZZER_PIN, TONE_FREQUENCY);
      controlLED(true);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      noTone(BUZZER_PIN);
      controlLED(false);
    }

    insertData(temperature, humidity, gas, rain);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Temp: ");
    display.print(temperature);
    display.println(" C");
    display.print("Humidity: ");
    display.print(humidity);
    display.println(" %");
    display.print("Gas: ");
    display.print(gas);
    display.println(" ppm");
    display.print("Rain: ");
    display.println(rain == 0 ? "Detected" : "None");
    display.display();

    lastTime = millis();
  }
}

void mqttInit() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(topic);
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
