#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>

// ===== CONFIGURAZIONE =====
const char* ssid = "WIFI_SID";
const char* password = "WIFI_PWD";
const char* ota_hostname = "PZEM004MQTT";

const uint8_t newAddress = 0x03; // <<< CHANGE FOR YOUR DESIDERED ADDRESS 

#define RX_PIN D5
#define TX_PIN D6
SoftwareSerial pzemSerial(RX_PIN, TX_PIN);
PZEM004Tv30 pzem(pzemSerial);  // indirizzo temporaneo lo imposta dinamicamente

ESP8266WebServer server(80);
String risultato = "In attesa di esecuzione...";

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}

void handleRoot() {
  risultato = "<h2>Scansione indirizzi Modbus...</h2>";
  bool trovato = false;

  for (uint8_t addr = 1; addr <= 247; addr++) {
    pzem.setAddress(addr);
    delay(100);

    float v = pzem.voltage();
    if (!isnan(v)) {
      risultato += "✅ Trovato modulo attivo a indirizzo <b>0x" + String(addr, HEX) + "</b> (Volt: " + String(v, 1) + "V)<br>";
      delay(200);

      bool ok = pzem.setAddress(newAddress);
      if (ok) {
        risultato += "➡️ Indirizzo cambiato con successo a <b>0x" + String(newAddress, HEX) + "</b><br>";
      } else {
        risultato += "❌ ERRORE: impossibile cambiare indirizzo<br>";
      }

      trovato = true;
      break; // basta il primo che risponde
    }

    yield();
  }

  if (!trovato) {
    risultato += "<br><strong>⚠️ Nessun modulo risponde al momento.</strong>";
  }

  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Set indirizzo PZEM</title></head><body>";
  html += risultato;
  html += "<br><br><a href='/'>↻ Riprova</a>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}
