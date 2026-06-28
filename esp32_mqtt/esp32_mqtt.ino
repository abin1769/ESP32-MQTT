#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h> // [cite: 1]
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <PubSubClient.h>  // [cite: 17]
#include <DHT.h>           // [cite: 17]

// Import Konfigurasi Kredensial WiFi & OTA dari File config.h (diabaikan oleh git)
#include "config.h"

WebServer server(80); // [cite: 3]
const char *serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"; // [cite: 4]
const char *csrfHeaders[2] = {"Origin", "Host"}; // [cite: 4]
static bool authenticated = false;               // [cite: 5]

// --- Konfigurasi MQTT ---
const char* mqtt_server = "broker.emqx.io"; // [cite: 19]
const int mqtt_port = 1883;                 // [cite: 19]
#define TOPIC_PREFIX "abin_esp32s3"         // [cite: 21]
const char* topic_status = TOPIC_PREFIX "/status"; // [cite: 21]
const char* topic_dht    = TOPIC_PREFIX "/sensor/dht"; // [cite: 21]
const char* topic_dist   = TOPIC_PREFIX "/sensor/distance"; // [cite: 22]
const char* topic_tele   = TOPIC_PREFIX "/telemetry"; // [cite: 22]
const char* topic_rgb    = TOPIC_PREFIX "/led/rgb"; // [cite: 23]
const char* topic_cmd    = TOPIC_PREFIX "/cmd"; // [cite: 23]
const char* topic_log    = TOPIC_PREFIX "/debug/log"; // [cite: 24]

WiFiClient espClient; // [cite: 28]
PubSubClient client(espClient); // [cite: 28]

// ==========================================
// 2. KONFIGURASI HARDWARE (SENSOR & LED)
// ==========================================
#define DHTPIN 4 // [cite: 24]
#define DHTTYPE DHT11 // [cite: 24]
DHT dht(DHTPIN, DHTTYPE); // [cite: 24]

#define TRIG_PIN 5 // [cite: 25]
#define ECHO_PIN 6 // [cite: 25]

// ==================== PILIH KONFIGURASI LED ====================
// Hapus tanda komentar '//' di bawah jika menggunakan LED RGB bawaan (Onboard) Sparkle IoT XH-S3E (WS2812B)
#define USE_ONBOARD_LED 

#ifdef USE_ONBOARD_LED
  // Jika RGB_BUILTIN belum terdefinisi oleh board manager, kita set ke 48 (pin default ESP32-S3)
  #ifndef RGB_BUILTIN
    #define RGB_BUILTIN 48
  #endif
#else
  // Jika menggunakan LED RGB eksternal biasa (bukan WS2812)
  #define RED_PIN 12 // [cite: 26]
  #define GREEN_PIN 13 // [cite: 26]
  #define BLUE_PIN 14 // [cite: 26]
  const bool IS_COMMON_ANODE = false; // [cite: 27]
#endif
// ===============================================================

unsigned long lastPublish = 0; // [cite: 28]
const unsigned long interval = 2000; // [cite: 28]

// Buffer Warna LED untuk dijalankan di loop utama (menghindari interrupt conflict RMT)
volatile bool colorChanged = false;
volatile int targetR = 0;
volatile int targetG = 0;
volatile int targetB = 0;

// ==========================================
// 3. FUNGSI-FUNGSI PENDUKUNG
// ==========================================

// Helper Log Debug ke Web & Serial
void logToWeb(String msg) {
  Serial.println(msg); // [cite: 30]
  if (client.connected()) {
    String formattedLog = "[" + String(millis() / 1000) + "s] " + msg; // [cite: 30]
    client.publish(topic_log, formattedLog.c_str()); // [cite: 31]
  }
}

// Setel warna RGB
void setRGBColor(int r, int g, int b) {
  #ifdef USE_ONBOARD_LED
    // Menggunakan fungsi built-in ESP32 core untuk mengontrol LED WS2812B bawaan
    neopixelWrite(RGB_BUILTIN, r, g, b);
  #else
    int outR = IS_COMMON_ANODE ? (255 - r) : r; 
    int outG = IS_COMMON_ANODE ? (255 - g) : g; 
    int outB = IS_COMMON_ANODE ? (255 - b) : b; 

    #if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
      ledcWrite(RED_PIN, outR); 
      ledcWrite(GREEN_PIN, outG); 
      ledcWrite(BLUE_PIN, outB); 
    #else
      ledcWrite(0, outR); 
      ledcWrite(1, outG); 
      ledcWrite(2, outB); 
    #endif
  #endif
}

// Callback penerima perintah MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  // PENTING: Salin topic ke local String SEGERA sebelum ter-overwrite oleh panggilan publish di logToWeb!
  String topicStr = String(topic);

  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    char c = (char)payload[i];
    if (c != '\0' && c != '\r' && c != '\n') {
      message += c;
    }
  }
  message.trim();
  
  // Debug Info
  logToWeb("[V2] MQTT Rcv [" + topicStr + "]: \"" + message + "\" (len=" + String(message.length()) + ")");
  logToWeb("[V2] DBG: topic_len=" + String(topicStr.length()) + ", topic_rgb_len=" + String(strlen(topic_rgb)) + ", topic_cmd_len=" + String(strlen(topic_cmd)));

  // Pencocokan topic menggunakan local String yang aman dari overwrite
  if (topicStr.endsWith("/led/rgb")) { // [cite: 41]
    int r = 0, g = 0, b = 0; // [cite: 41]
    if (sscanf(message.c_str(), "%d,%d,%d", &r, &g, &b) == 3) { // [cite: 42]
      targetR = r;
      targetG = g;
      targetB = b;
      colorChanged = true;
      logToWeb("[V2] RGB Updated -> R:" + String(r) + " G:" + String(g) + " B:" + String(b)); // [cite: 43]
    } else {
      logToWeb("[V2] Gagal parsing format warna RGB!"); // [cite: 44]
    }
  }

  else if (topicStr.endsWith("/cmd")) { // [cite: 45]
    if (message == "reboot") {
      logToWeb("[V2] Sistem direboot remote dari Web..."); // [cite: 45]
      delay(1000); // [cite: 46]
      ESP.restart(); // [cite: 46]
    } else if (message == "ping") {
      logToWeb("[V2] Pong! ESP32-S3 is alive."); // [cite: 46]
    } else if (message == "led_off") { // [cite: 47]
      targetR = 0;
      targetG = 0;
      targetB = 0;
      colorChanged = true;
      logToWeb("[V2] Semua warna LED dimatikan."); // [cite: 47]
    } else {
      logToWeb("[V2] Command tidak dikenali: " + message); // [cite: 48]
    }
  }
}

// Reconnect MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection..."); // [cite: 49]
    String clientId = "ESP32S3-Client-" + String(random(0xffff), HEX); // [cite: 50]
    if (client.connect(clientId.c_str(), topic_status, 1, true, "offline")) { // [cite: 51]
      Serial.println("connected"); // [cite: 51]
      client.publish(topic_status, "online", true); // [cite: 52]
      client.subscribe(topic_rgb); // [cite: 53]
      client.subscribe(topic_cmd); // [cite: 53]
      logToWeb("ESP32-S3 MQTT Handshake Berhasil! Siap dimonitor."); // [cite: 53]
    } else {
      Serial.print("failed, rc="); // [cite: 54]
      Serial.print(client.state()); // [cite: 54]
      Serial.println(" try again in 5 seconds"); // [cite: 54]
      delay(5000); // [cite: 54]
    }
  }
}

// Koneksi WiFi Setup
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_AP_STA); // Set mode untuk OTA [cite: 5]
  WiFi.begin(ssid, password); // [cite: 31]
  while (WiFi.status() != WL_CONNECTED) { // [cite: 32]
    delay(500); // [cite: 32]
    Serial.print("."); // [cite: 32]
  }
  Serial.println(""); // [cite: 32]
  Serial.println("WiFi connected!"); // [cite: 32]
  Serial.print("IP Address: "); // [cite: 32]
  Serial.println(WiFi.localIP()); // [cite: 32]
}

// Baca Jarak HC-SR04
float getDistance() {
  digitalWrite(TRIG_PIN, LOW); // [cite: 63]
  delayMicroseconds(2); // [cite: 63]
  digitalWrite(TRIG_PIN, HIGH); // [cite: 64]
  delayMicroseconds(10); // [cite: 64]
  digitalWrite(TRIG_PIN, LOW); // [cite: 64]
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // [cite: 64]
  if (duration == 0) return -1; // [cite: 65]
  return duration * 0.0343 / 2.0; // [cite: 65]
}

// Fungsi Otentikasi Kustom untuk Menghindari Bug Library WebServer ESP32
#include <base64.h>
bool customAuthenticate() {
  if (!server.hasHeader("Authorization")) {
    return false;
  }
  String authHeader = server.header("Authorization");
  authHeader.trim();
  
  String credentials = String(authUser) + ":" + String(authPass);
  String expected = "Basic " + base64::encode(credentials);
  expected.replace("\n", "");
  expected.replace("\r", "");
  expected.trim();
  
  // Cetak hasil perbandingan ke konsol web untuk verifikasi
  logToWeb("[V2] Auth Check - Recv: '" + authHeader + "' | Expected: '" + expected + "'");
  
  return authHeader.equalsIgnoreCase(expected);
}

// ==========================================
// 4. SETUP UTAMA
// ==========================================
void setup(void) {
  Serial.begin(115200); // [cite: 5, 55]
  
  // Setup Pin Mode Hardware
  pinMode(TRIG_PIN, OUTPUT); // [cite: 55]
  pinMode(ECHO_PIN, INPUT);  // [cite: 55]

  #ifdef USE_ONBOARD_LED
    // neopixelWrite tidak memerlukan inisialisasi PWM khusus
    setRGBColor(0, 0, 0);
  #else
    pinMode(RED_PIN, OUTPUT); // [cite: 57]
    pinMode(GREEN_PIN, OUTPUT); // [cite: 57]
    pinMode(BLUE_PIN, OUTPUT); // [cite: 57]

    #if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
      ledcAttach(RED_PIN, 5000, 8); // [cite: 57]
      ledcAttach(GREEN_PIN, 5000, 8); // [cite: 58]
      ledcAttach(BLUE_PIN, 5000, 8); // [cite: 58]
    #else
      ledcSetup(0, 5000, 8); // [cite: 58]
      ledcSetup(1, 5000, 8); // [cite: 59]
      ledcSetup(2, 5000, 8); // [cite: 60]
      ledcAttachPin(RED_PIN, 0); // [cite: 61]
      ledcAttachPin(GREEN_PIN, 1); // [cite: 61]
      ledcAttachPin(BLUE_PIN, 2); // [cite: 61]
    #endif
    setRGBColor(0, 0, 0); // [cite: 62]
  #endif

  dht.begin(); // [cite: 63]
  setup_wifi(); // Sambungkan WiFi [cite: 63]

  // Setup Web OTA Server
  MDNS.begin(host); // [cite: 6]
  server.collectHeaders(csrfHeaders, 2); // [cite: 6]
  
  server.on("/", HTTP_GET, []() { // [cite: 7]
    if (!customAuthenticate()) return server.requestAuthentication(); // [cite: 7]
    server.sendHeader("Connection", "close"); // [cite: 7]
    server.send(200, "text/html", serverIndex); // [cite: 7]
  });

  server.on("/update", HTTP_POST, []() { // [cite: 8]
    if (!authenticated) {
      authenticated = customAuthenticate();
    }
    logToWeb("[V2] POST Handler - AuthVar: " + String(authenticated) + ", HasAuth: " + String(server.hasHeader("Authorization")));
    if (!authenticated) return server.requestAuthentication(); // [cite: 8]
    server.sendHeader("Connection", "close"); // [cite: 8]
    if (Update.hasError()) { // [cite: 8]
      server.send(200, "text/plain", "FAIL"); // [cite: 8]
    } else {
      server.send(200, "text/plain", "Success! Rebooting..."); // [cite: 8]
      delay(500); // [cite: 9]
      ESP.restart(); // [cite: 9]
    }
  }, []() {
    HTTPUpload &upload = server.upload(); // [cite: 9]
    if (upload.status == UPLOAD_FILE_START) { // [cite: 9]
      Serial.setDebugOutput(true); // [cite: 9]
      
      // LOG HEADERS FOR DIAGNOSTICS
      String headerLog = "Headers: ";
      for (int i = 0; i < server.headers(); i++) {
        headerLog += server.headerName(i) + ": " + server.header(i) + " | ";
      }
      logToWeb("[V2] DBG " + headerLog);

      authenticated = customAuthenticate(); // [cite: 9]
      if (!authenticated) { Serial.println("Authentication fail!"); return; } // [cite: 9, 10]
      String origin = server.header(String(csrfHeaders[0])); // [cite: 10]
      String host = server.header(String(csrfHeaders[1])); // [cite: 10]
      String expectedOrigin = String("http://") + host; // [cite: 10]
      if (origin != expectedOrigin) { // [cite: 10]
        Serial.printf("Wrong origin received! Expected: %s, Received: %s\n", expectedOrigin.c_str(), origin.c_str()); // [cite: 10, 11]
        authenticated = false; return; // [cite: 11]
      }
      Serial.printf("Update: %s\n", upload.filename.c_str()); // [cite: 11]
      if (!Update.begin()) Update.printError(Serial); // [cite: 11]
    } else if (authenticated && upload.status == UPLOAD_FILE_WRITE) { // [cite: 12]
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial); // [cite: 12]
    } else if (authenticated && upload.status == UPLOAD_FILE_END) { // [cite: 12]
      if (Update.end(true)) Serial.printf("Update Success: %lu\nRebooting...\n", (unsigned long)upload.totalSize); // [cite: 12, 13]
      else Update.printError(Serial); // [cite: 13]
      Serial.setDebugOutput(false); // [cite: 13]
    } else if (authenticated) {
      Serial.printf("Update Failed Unexpectedly: status=%d\n", upload.status); // [cite: 13]
    }
  }); // [cite: 14]
  
  server.begin(); // [cite: 14]
  MDNS.addService("http", "tcp", 80); // [cite: 14]
  Serial.printf("Web OTA Ready! Open http://%s.local in your browser\n", host); // [cite: 15]

  // Setup MQTT Client
  client.setServer(mqtt_server, mqtt_port); // [cite: 63]
  client.setCallback(callback); // [cite: 63]
}

// ==========================================
// 5. LOOP UTAMA
// ==========================================
void loop(void) {
  server.handleClient(); // WAJIB ADA UNTUK WEB OTA 
  delay(2);  //allow the cpu to switch to other tasks // [cite: 16]

  if (!client.connected()) { // 
    reconnect(); // 
  }
  client.loop(); // 

  // Update warna LED di loop utama agar terhindar dari interupsi paket jaringan WiFi
  if (colorChanged) {
    colorChanged = false;
    setRGBColor(targetR, targetG, targetB);
  }

  unsigned long now = millis(); // 
  if (now - lastPublish >= interval) { // 
    lastPublish = now; // 

    // Baca Sensor DHT11
    float t = dht.readTemperature(); // [cite: 68]
    float h = dht.readHumidity(); // [cite: 68]
    if (!isnan(t) && !isnan(h)) { // [cite: 69]
      String dht_payload = "{\"temp\":" + String(t, 1) + ",\"humi\":" + String(h, 0) + "}"; // [cite: 69]
      client.publish(topic_dht, dht_payload.c_str()); // [cite: 70]
    } else {
      logToWeb("[WARNING] Gagal membaca DHT11!"); // [cite: 70]
    }

    // Baca HC-SR04
    float dist = getDistance(); // [cite: 71]
    if (dist >= 0) { // [cite: 72]
      String dist_payload = String(dist, 1); // [cite: 72]
      client.publish(topic_dist, dist_payload.c_str()); // [cite: 72]
    }

    // Baca Telemetry
    uint32_t freeHeap = ESP.getFreeHeap(); // [cite: 73]
    uint32_t totalHeap = ESP.getHeapSize(); // [cite: 74]
    long rssi = WiFi.RSSI(); // [cite: 74]
    uint32_t cpuFreq = ESP.getCpuFreqMHz(); // [cite: 74]
    uint32_t uptime = millis() / 1000; // [cite: 74]
    
    float coreTemp = 0; // [cite: 75]
    #ifdef SOC_TEMP_SENSOR_SUPPORTED
       coreTemp = temperatureRead(); // [cite: 76]
    #else
       coreTemp = temperatureRead(); // [cite: 77]
    #endif

    String ipStr = WiFi.localIP().toString();

    String tele_payload = "{" // [cite: 78]
                          "\"free_heap\":" + String(freeHeap) + "," // [cite: 78]
                          "\"total_heap\":" + String(totalHeap) + "," // [cite: 78]
                          "\"rssi\":" + String(rssi) + "," // [cite: 79]
                          "\"cpu\":" + String(cpuFreq) + "," // [cite: 79]
                          "\"uptime\":" + String(uptime) + "," // [cite: 79]
                          "\"core_temp\":" + String(coreTemp, 1) + "," // [cite: 79]
                          "\"ip\":\"" + ipStr + "\""
                          "}"; // [cite: 80]
    client.publish(topic_tele, tele_payload.c_str()); // [cite: 81]
  }
}
