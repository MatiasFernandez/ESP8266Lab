#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <DHT.h>                  //Read DHT sensors data
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define DHT_PIN D2     // what digital pin the DHT22 is conected to
#define DHT_TYPE DHT22   // there are multiple kinds of DHT sensors

// Redefine DHT init delay to improve reliability of reading DHT22 data when using
// ESP8266 with DHT v1.3.7 library. See: https://github.com/adafruit/DHT-sensor-library/issues/116
#define DHT_INIT_DELAY_USEC 60

#define DHT_MEASUREMENT_DELAY_MS 2000

#define AP_SSID cosito
#define AP_PASSWORD 1234567890

struct DHTMeasurement {
  float humidity;
  float temperature;
  float heatIndex;
};

DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastMeasurementAt = 0;
const char* mqtt_server = "192.168.100.103";
WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output

  WiFiManager wifiManager;

  wifiManager.autoConnect("cosito", "1234567890");

  setupSerialMonitor();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
    
  Serial.println("Device Started");
  
  dht.begin(DHT_INIT_DELAY_USEC);
  
  Serial.println("-------------------------------------");
  Serial.println("Running DHT!");
  Serial.println("-------------------------------------");

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  if(shouldTakeDHTMeasurement()) {
    DHTMeasurement measurement = readDHTSensor();

    // Check if any reads failed and exit early (to try again).
    if (isnan(measurement.humidity) || isnan(measurement.temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    reportMeasurement(measurement);

    Serial.print("Humidity: ");
    Serial.print(measurement.humidity);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(measurement.temperature);
    Serial.print(" *C ");
    Serial.print("Heat index: ");
    Serial.print(measurement.heatIndex);
    Serial.println(" *C ");
  }
}

void setupSerialMonitor() {  
  Serial.begin(115200);
  Serial.setTimeout(2000);

  // Wait for serial to initialize.
  while(!Serial) {
    delay(50);
  }
}


DHTMeasurement readDHTSensor() {
  DHTMeasurement measurement;
  
  measurement.humidity = dht.readHumidity();
  measurement.temperature = dht.readTemperature();
  
  measurement.heatIndex = dht.computeHeatIndex(measurement.temperature, measurement.humidity, false);  

  lastMeasurementAt = millis();
  
  return measurement;
}

void reportMeasurement(DHTMeasurement measurement) {
  StaticJsonDocument<JSON_OBJECT_SIZE(3)> json;
  
  json["humidity"] = measurement.humidity;
  json["temperature"] = measurement.temperature;
  json["heatIndex"] = measurement.heatIndex;
  
  String message;

  serializeJson(json, message);

  client.publish("outTopic", message.c_str());
}

boolean shouldTakeDHTMeasurement() {
  return millis() - lastMeasurementAt > DHT_MEASUREMENT_DELAY_MS;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
