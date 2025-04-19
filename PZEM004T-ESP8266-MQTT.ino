#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>
#include <PubSubClient.h>

// ===== CONFIG =====
const char* ssid = "WIFI_NAME";
const char* password = "WIFI_PASSWORD";
const char* ota_hostname = "PZEM004MQTT";

const char* mqtt_host = "IP_MQTT";
const int mqtt_port = 1883;
const char* mqtt_user = "MQTT_USERNAME";
const char* mqtt_pass = "MQTT_PASSWORD";
const String mqtt_base_topic = "homeassistant";
// ===================


#define RX_PIN D5
#define TX_PIN D6
SoftwareSerial pzemSerial(RX_PIN, TX_PIN);

WiFiClient espClient;
PubSubClient mqttClient(espClient);
ESP8266WebServer server(80);

PZEM004Tv30 pzem1(pzemSerial, 0x01);
PZEM004Tv30 pzem2(pzemSerial, 0x02);
PZEM004Tv30 pzem3(pzemSerial, 0x03);

struct PZEMData {
  float voltage, current, power, energy, frequency, pf;
};

PZEMData data1, data2, data3;

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int wifi_attempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_attempts < 5) {
    delay(1000);
    wifi_attempts++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    ESP.restart();
  }

  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/reboot", []() {
    server.send(200, "text/html", "<html><body><h2>Riavvio ESP...</h2></body></html>");
    delay(1000);
    ESP.restart();
  });

  server.begin();

  mqttClient.setServer(mqtt_host, mqtt_port);
  mqttClient.setBufferSize(512);  // Aumentiamo la dimensione dei messaggi MQTT
  connectToMQTT();

  publishDiscovery("pzem1", 0x01);
  publishDiscovery("pzem2", 0x02);
  publishDiscovery("pzem3", 0x03);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  mqttClient.loop();

  data1 = readPZEM(pzem1, "PZEM1");
  yield(); delay(1000);
  publishData("pzem1", data1);

  data2 = readPZEM(pzem2, "PZEM2");
  yield(); delay(1000);
  publishData("pzem2", data2);

  data3 = readPZEM(pzem3, "PZEM3");
  yield(); delay(1000);
  publishData("pzem3", data3);

  yield(); delay(5000);
}

PZEMData readPZEM(PZEM004Tv30 &pzem, const String &label) {
  PZEMData d;
  d.voltage = pzem.voltage();
  d.current = pzem.current();
  d.power = pzem.power();
  d.energy = pzem.energy();
  d.frequency = pzem.frequency();
  d.pf = pzem.pf();

  if (isnan(d.voltage)) {
    delay(1000);
    ESP.restart();
  } 

  return d;
}

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>PZEM Debug</title></head><body>";
  html += "<h2>PZEM-004T v3 - MQTT + Web</h2>";
  html += formatPZEMTable("PZEM1", data1);
  html += formatPZEMTable("PZEM2", data2);
  html += formatPZEMTable("PZEM3", data3);
  html += "<br><button onclick='location.reload()'>Aggiorna</button>";
  html += "<br><br><button onclick=\"location.href='/reboot'\">Riavvia ESP</button>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

String formatPZEMTable(const String &label, const PZEMData &d) {
  String html = "<h3>" + label + "</h3><table border='1' cellpadding='5'>";
  if (isnan(d.voltage)) {
    html += "<tr><td colspan='2'>Errore lettura</td></tr></table>";
    return html;
  }
  html += "<tr><td>Volt</td><td>" + String(d.voltage, 2) + " V</td></tr>";
  html += "<tr><td>Corrente</td><td>" + String(d.current, 2) + " A</td></tr>";
  html += "<tr><td>Potenza</td><td>" + String(d.power, 2) + " W</td></tr>";
  html += "<tr><td>Energia</td><td>" + String(d.energy, 2) + " Wh</td></tr>";
  html += "<tr><td>Frequenza</td><td>" + String(d.frequency, 2) + " Hz</td></tr>";
  html += "<tr><td>Fattore Potenza</td><td>" + String(d.pf, 2) + "</td></tr></table>";
  return html;
}

void connectToMQTT() {
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    if (mqttClient.connect(ota_hostname, mqtt_user, mqtt_pass)) {
      Serial.println(" connesso!");
      return;
    } else {
      Serial.print(" fallita, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" - riprovo in 2 secondi");
      delay(2000);
      attempts++;
    }
  }
  if (!mqttClient.connected()) {
    delay(1000);
    ESP.restart();
  }
}

void publishDiscovery(const String& name, uint8_t addr) {
  const String keys[] = {"voltage", "current", "power", "energy", "frequency", "pf"};
  const String units[] = {"V", "A", "W", "kWh", "Hz", ""};
  const String device_class[] = {"voltage", "current", "power", "energy", "frequency", "power_factor"};

for (int i = 0; i < 6; i++) {

  String key = keys[i];
  String topic = mqtt_base_topic + "/sensor/" + name + "_" + key + "/config";
  String payload = "{";

  payload += "\"name\": \"" + name + "_" + key + "\",";
  payload += "\"state_topic\": \"" + mqtt_base_topic + "/" + name + "/" + key + "\",";
  payload += "\"unit_of_measurement\": \"" + units[i] + "\",";

  payload += "\"device_class\": \"" + device_class[i] + "\"";

  if (key.equals("energy")) {
    payload += ",\"state_class\": \"total_increasing\"";
  }

  payload += ",\"unique_id\": \"" + name + "_" + key + "\"";
  payload += ",\"device\": {\"identifiers\": [\"" + name + "\"], \"name\": \"" + name + "\"}";
  payload += "}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

}

void publishData(const String& name, const PZEMData& d) {
  mqttClient.publish((mqtt_base_topic + "/" + name + "/voltage").c_str(), String(d.voltage).c_str(), true);
  mqttClient.publish((mqtt_base_topic + "/" + name + "/current").c_str(), String(d.current).c_str(), true);
  mqttClient.publish((mqtt_base_topic + "/" + name + "/power").c_str(), String(d.power).c_str(), true);
  mqttClient.publish((mqtt_base_topic + "/" + name + "/energy").c_str(), String(d.energy / 1000.0, 3).c_str(), true);
  mqttClient.publish((mqtt_base_topic + "/" + name + "/frequency").c_str(), String(d.frequency).c_str(), true);
  mqttClient.publish((mqtt_base_topic + "/" + name + "/pf").c_str(), String(d.pf).c_str(), true);
}