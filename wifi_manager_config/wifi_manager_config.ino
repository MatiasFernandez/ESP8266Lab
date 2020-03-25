#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define AP_SSID "cosito"
#define AP_PASSWORD "1234567890"

#define WIFI_RESET_PIN D1

struct Config {
  char mqtt_server[40];
  char mqtt_port[6];
};

const char *configFilename = "/config.txt";
Config config;                       

bool shouldSaveConfig = false;

void saveConfigCallback () {
  Serial.println(F("New configuration provided."));
  shouldSaveConfig = true;
}

void setupConfiguration() {
  Serial.println(F("Mounting Filesystem..."));

  if (SPIFFS.begin()) {
    Serial.println(F("Filesystem successfully mounted."));
    loadConfiguration(configFilename, config);
  } else {
    Serial.println(F("Failed to mount Filesystem."));
  }
}

void setupWifi() {
  Serial.println(F("Initializing Wifi connection."));

  WiFiManager wifiManager;

  if(digitalRead(WIFI_RESET_PIN)) {
    Serial.println(F("Resetting Wifi Settings."));
    wifiManager.resetSettings();
  }
  
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_mqtt_server("mqtt_hostname", "mqtt hostname", config.mqtt_server, sizeof(config.mqtt_server));
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt port", config.mqtt_port, sizeof(config.mqtt_port));

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  
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
    strcpy(config.mqtt_server, custom_mqtt_server.getValue());
    strcpy(config.mqtt_port, custom_mqtt_port.getValue());
  
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

void loadConfiguration(const char *filename, Config &config) {
  File file = SPIFFS.open(filename, "r");

  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, file);
  
  if (error) {
    Serial.print(F("Failed to read file, using default configuration: "));
    Serial.println(error.c_str());
  }

  strlcpy(config.mqtt_server, doc["mqtt_server"] | "localhost", sizeof(config.mqtt_server));
  strlcpy(config.mqtt_port, doc["mqtt_port"] | "8080", sizeof(config.mqtt_port));
  
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

  doc["mqtt_server"] = config.mqtt_server;
  doc["mqtt_port"] = config.mqtt_port;

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

void setup() {
  setupSerialMonitor();
  setupConfiguration();
  setupWifi();

  Serial.println(F("Print config file..."));
  printFile(configFilename);
}

void loop() {
}
