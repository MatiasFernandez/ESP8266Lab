#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>

#include <NTPClient.h>            //NTP Client for time/date synchronization
#include <WiFiUdp.h>              //Needed by NTP client

#include <WebSocketsServer.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define AP_SSID "cosito"
#define AP_PASSWORD "1234567890"

#define WIFI_RESET_PIN D1
#define SMART_SWITCH_PIN BUILTIN_LED

struct Config {
  char ntp_server[40];
};

const char *configFilename = "/config.txt";
Config config;                       

bool shouldSaveConfig = false;

struct SmartSwitch {
  boolean isOn = false;
};

SmartSwitch smartSwitch;    

const long timeZoneOffset = -3 * 60 * 60; // GMT-3

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, timeZoneOffset);

boolean clockSynchronized = false;

WebSocketsServer webSocket(81);
ESP8266WebServer webServer;

void saveConfigCallback () {
  Serial.println(F("New configuration provided."));
  shouldSaveConfig = true;
}

void setupClock() {
  timeClient.setPoolServerName(config.ntp_server);
  timeClient.begin();
}

void setupConfiguration() {
  Serial.println(F("Mounting Filesystem..."));

  if (SPIFFS.begin()) {
    Serial.println(F("Filesystem successfully mounted."));
    loadConfiguration(configFilename, config);
  } else {
    Serial.println(F("Failed to mount Filesystem."));
  }

  Serial.println(F("Current configuration:"));
  printFile(configFilename);
}

void setupPins() {
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(SMART_SWITCH_PIN, OUTPUT);
}

void setupWifi() {
  Serial.println(F("Initializing Wifi connection."));

  WiFi.hostname("SmartSwitch");

  WiFiManager wifiManager;

  if(!digitalRead(WIFI_RESET_PIN)) {
    Serial.println(F("Resetting Wifi Settings."));
    wifiManager.resetSettings();
  }
  
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_ntp_server("ntp_server", "ntp server", config.ntp_server, sizeof(config.ntp_server));

  wifiManager.addParameter(&custom_ntp_server);
  
  wifiManager.setTimeout(120);

  if (!wifiManager.autoConnect(AP_SSID, AP_PASSWORD)) {
    Serial.println(F("Failed to connect and hit timeout."));
    delay(3000);
    
    ESP.reset();
    delay(5000);
  }

  Serial.print(F("Connected to Wifi network: "));
  Serial.println(WiFi.SSID());
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());

  if (shouldSaveConfig) {
    strcpy(config.ntp_server, custom_ntp_server.getValue());
  
    saveConfiguration(configFilename, config);
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

void setupWebServer() {
  Serial.println(F("Initializing web server"));
  
  webServer.serveStatic("/", SPIFFS, "/index.html");
  webServer.serveStatic("/app.css", SPIFFS, "/app.css");

  webServer.begin();

  Serial.println(F("Web server initialized"));
}

void setupMDns() {
  if (!MDNS.begin("smartswitch")) {             
     Serial.println(F("Failed to start mDNS service"));
   }
   Serial.println(F("mDNS started"));

   MDNS.addService("http", "tcp", 80);
}

void setupWebSocketsServer() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);  
}

void loadConfiguration(const char *filename, Config &config) {
  File file = SPIFFS.open(filename, "r");

  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, file);
  
  if (error) {
    Serial.print(F("Failed to read file, using default configuration: "));
    Serial.println(error.c_str());
  }

  strlcpy(config.ntp_server, doc["ntp_server"] | "time.cloudflare.com", sizeof(config.ntp_server));
  
  file.close();
}

void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove(filename);

  File file = SPIFFS.open(filename, "w");
  
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  StaticJsonDocument<256> doc;

  doc["ntp_server"] = config.ntp_server;

  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  file.close();
}

void printFile(const char *filename) {
  File file = SPIFFS.open(filename, "r");
  
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  while (file.available()) {
    Serial.print((char)file.read());
  }
  
  Serial.println();

  file.close();
}

void updateClock() {
  boolean updated = timeClient.update();

  if (clockSynchronized != true && updated == true) {
    clockSynchronized = true;
  }
}

void reportStatus() {
  StaticJsonDocument<JSON_OBJECT_SIZE(3)> json;
  
  json["type"] = "statusReport";
  json["clockSynchronized"] = clockSynchronized;
  json["isOn"] = smartSwitch.isOn;
  
  String webSocketMessage;
  
  serializeJson(json, webSocketMessage);
  
  webSocket.broadcastTXT(webSocketMessage);
}

void toggleSwitch(boolean enable) {
  smartSwitch.isOn = enable;
  
  if(enable) {
    digitalWrite(SMART_SWITCH_PIN, LOW);
  } else {
    digitalWrite(SMART_SWITCH_PIN, HIGH);
  }
  
  reportStatus();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  switch (type) {
    case WStype_DISCONNECTED: // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        reportStatus();
      }
      break;
    case WStype_TEXT:  { // if new text data is received
        Serial.printf("[%u] get Text: %s\n", num, payload);
        StaticJsonDocument<256> doc;
  
        DeserializationError error = deserializeJson(doc, payload);
    
        if (error) {
          Serial.print(F("Failed to parse ws message: "));
          Serial.println(error.c_str());
        }
                
        toggleSwitch(doc["enable"]);
      }
      break;
  }
}

void setup() {
  setupPins();
  setupSerialMonitor();

  Serial.print(F("Startup reason:"));
  Serial.println(ESP.getResetReason());
  
  setupConfiguration();
  setupWifi();
  setupClock();
  setupWebServer();
  setupWebSocketsServer(); 
  setupMDns();

  toggleSwitch(false);
  
  Serial.println(F("Device Started"));
}

void loop() {
  webSocket.loop();
  webServer.handleClient();
  MDNS.update();
  
  updateClock();
}
