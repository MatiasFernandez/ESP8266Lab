#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>

ESP8266WebServer webServer;

char indexHtml[] PROGMEM = ""
"<html>"
"<body>Hello!</body>"
"</html>";

void setup() {
  WiFiManager wifiManager;

  wifiManager.autoConnect("cosito", "1234567890");

  setupSerialMonitor();
  Serial.println("Serial monitor started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  setupMDns();
  setupWebServer();

  Serial.println("Device Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  webServer.handleClient();
  MDNS.update();
}

void setupSerialMonitor() {
  Serial.begin(115200);
  Serial.setTimeout(2000);

  // Wait for serial to initialize.
  while(!Serial) {
    delay(50);
  }
}

void setupWebServer() {
  Serial.println("Initializing web server");
  
  webServer.on("/", [](){
    webServer.send_P(200, "text/html", indexHtml);
  });

  webServer.begin();

  Serial.println("Web server initialized");
}

void setupMDns() {
  if (!MDNS.begin("cosito")) {             
     Serial.println("Failed to start mDNS service");
   }
   Serial.println("mDNS started");

   MDNS.addService("http", "tcp", 80);
}
