#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <WebSocketsServer.h>

#define LDR_PIN A0     // what digital pin the DHT22 is conected to

#define MEASUREMENT_DELAY_MS 100

#define AP_SSID cosito
#define AP_PASSWORD 1234567890

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

void setup() {
  WiFiManager wifiManager;

  wifiManager.autoConnect("cosito", "1234567890");

  setupSerialMonitor();

  setupWebServer();
  
  webSocket.begin();
  
  Serial.println("Device Started");
}

void loop() {
  webSocket.loop();
  webServer.handleClient();

  int measurement = analogRead(LDR_PIN);
  
  reportMeasurement(measurement);

  Serial.println(measurement);

  delay(500);
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

void reportMeasurement(int measurement) {
  String webSocketMessage = String(measurement);
  webSocket.broadcastTXT(webSocketMessage);
}
