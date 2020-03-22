#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <DHT.h>                  //Read DHT sensors data
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

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
WebSocketsServer webSocket(81);
ESP8266WebServer webServer;

char indexHtml[] PROGMEM = R"=====(
<html>
<head>
<script>
var socket;
function init() {
  socket = new WebSocket('ws://' + window.location.hostname + ':81');
  socket.onmessage = function(event) {
    document.getElementById("console").value += event.data + "\n";
  }
}
</script>
</head>
<body onload="javascript:init();">
<textarea id="console"></textarea>
</body>
</html>
)=====";

unsigned long lastMeasurementAt = 0;

void setup() {
  WiFiManager wifiManager;

  wifiManager.autoConnect("cosito", "1234567890");

  setupSerialMonitor();

  setupWebServer();
  
  webSocket.begin();
  
  Serial.println("Device Started");
  
  dht.begin(DHT_INIT_DELAY_USEC);
  
  Serial.println("-------------------------------------");
  Serial.println("Running DHT!");
  Serial.println("-------------------------------------");

}

void loop() {
  webSocket.loop();
  webServer.handleClient();
  
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
  while(!Serial) { }
}

void setupWebServer() {
  Serial.println("Initializing web server");
  
  webServer.on("/", [](){
    webServer.send_P(200, "text/html", indexHtml);
  });

  webServer.begin();

  Serial.println("Web server initialized");
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
  
  String webSocketMessage;
  
  serializeJson(json, webSocketMessage);
  
  webSocket.broadcastTXT(webSocketMessage);
}

boolean shouldTakeDHTMeasurement() {
  return millis() - lastMeasurementAt > DHT_MEASUREMENT_DELAY_MS;
}
