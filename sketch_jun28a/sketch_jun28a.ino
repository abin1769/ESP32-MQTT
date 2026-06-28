#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// --- KONFIGURASI WIFI & MQTT ---
const char* ssid = "Aulia Wulandari";          // <--- UBAH DENGAN SSID WIFI ANDA
const char* password = "FIDELYAAFRA";  // <--- UBAH DENGAN PASSWORD WIFI ANDA
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// Gunakan prefix unik agar tidak bentrok dengan pengguna EMQX lain di seluruh dunia!
#define TOPIC_PREFIX "abin_esp32s3"
const char* topic_status = TOPIC_PREFIX "/status"; // LWT (Last Will & Testament)
const char* topic_dht    = TOPIC_PREFIX "/sensor/dht";
const char* topic_dist   = TOPIC_PREFIX "/sensor/distance";
const char* topic_tele   = TOPIC_PREFIX "/telemetry";
const char* topic_rgb    = TOPIC_PREFIX "/led/rgb";
const char* topic_cmd    = TOPIC_PREFIX "/cmd";
const char* topic_log    = TOPIC_PREFIX "/debug/log";

// --- DEKLARASI HARDWARE & CONFIG LED ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define TRIG_PIN 5
#define ECHO_PIN 6

// ==================== PILIH KONFIGURASI LED ====================
// Hapus tanda komentar '//' di bawah jika menggunakan LED Digital WS2812B (onboard ESP32-S3 default/NeoPixel)
// #define USE_NEOPIXEL 

#ifdef USE_NEOPIXEL
  #include <Adafruit_NeoPixel.h>
  #define NEOPIXEL_PIN 48 // GPIO 48 atau 38 (onboard LED bawaan ESP32-S3 DevKit)
  #define NEOPIXEL_NUM 1
  Adafruit_NeoPixel pixels(NEOPIXEL_NUM, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#else
  // Jika menggunakan LED RGB eksternal biasa (bukan WS2812)
  #define RED_PIN 12
  #define GREEN_PIN 13
  #define BLUE_PIN 14
  // Ubah ke true jika LED eksternal Anda adalah Common Anode (VCC bersama).
  // Ubah ke false jika LED eksternal Anda adalah Common Cathode (GND bersama).
  const bool IS_COMMON_ANODE = false;
#endif
// ===============================================================

// --- DEKLARASI GLOBAL ---
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;
const unsigned long interval = 2000; // Kirim data sensor setiap 2 detik

// Helper untuk mengirim Log Debug ke Serial Monitor FISIK dan WEB sekaligus!
void logToWeb(String msg) {
  Serial.println(msg);
  if (client.connected()) {
    String formattedLog = "[" + String(millis() / 1000) + "s] " + msg;
    client.publish(topic_log, formattedLog.c_str());
  }
}

// Fungsi reconnect WiFi & MQTT
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Fungsi untuk menyetel warna LED RGB menggunakan LEDC (native ESP32 PWM)
void setRGBColor(int r, int g, int b) {
  #ifdef USE_NEOPIXEL
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
  #else
    // Jika Common Anode, balik nilai PWM (255 - value)
    int outR = IS_COMMON_ANODE ? (255 - r) : r;
    int outG = IS_COMMON_ANODE ? (255 - g) : g;
    int outB = IS_COMMON_ANODE ? (255 - b) : b;

    #if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
      // Core v3.x API
      ledcWrite(RED_PIN, outR);
      ledcWrite(GREEN_PIN, outG);
      ledcWrite(BLUE_PIN, outB);
    #else
      // Core v2.x API
      ledcWrite(0, outR);
      ledcWrite(1, outG);
      ledcWrite(2, outB);
    #endif
  #endif
}

// Fungsi Callback saat menerima pesan MQTT dari Laravel Dashboard
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  logToWeb("MQTT Rcv [" + String(topic) + "]: " + message);

  // 1. KONTROL RGB LED (Format payload: "R,G,B" contoh: "255,128,0")
  if (String(topic) == String(topic_rgb)) {
    int r = 0, g = 0, b = 0;
    if (sscanf(message.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
      setRGBColor(r, g, b);
      logToWeb("RGB Updated -> R:" + String(r) + " G:" + String(g) + " B:" + String(b));
    } else {
      logToWeb("Gagal parsing format warna RGB!");
    }
  }

  // 2. KONTROL COMMAND DEBUG (Misal: reboot, led_off, dll)
  if (String(topic) == String(topic_cmd)) {
    if (message == "reboot") {
      logToWeb("Sistem direboot remote dari Web...");
      delay(1000);
      ESP.restart();
    } else if (message == "ping") {
      logToWeb("Pong! ESP32-S3 is alive.");
    } else if (message == "led_off") {
      setRGBColor(0, 0, 0);
      logToWeb("Semua warna LED dimatikan.");
    } else {
      logToWeb("Command tidak dikenali: " + message);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Bikin client ID unik setiap restart
    String clientId = "ESP32S3-Client-" + String(random(0xffff), HEX);
    
    // Mengatur Last Will & Testament (LWT)
    // Jika ESP tiba-tiba mati/putus internet, Broker otomatis akan menyiarkan status "offline"
    if (client.connect(clientId.c_str(), topic_status, 1, true, "offline")) {
      Serial.println("connected");
      
      // Publish status "online" dengan bendera RETAIN = true
      client.publish(topic_status, "online", true);
      
      // Subscribe ke topik kontrol
      client.subscribe(topic_rgb);
      client.subscribe(topic_cmd);
      
      logToWeb("ESP32-S3 MQTT Handshake Berhasil! Siap dimonitor.");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Setup Pin Mode
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  #ifdef USE_NEOPIXEL
    pixels.begin();
    pixels.clear();
    pixels.show();
  #else
    // PENTING: Wajib setel pinMode ke OUTPUT untuk melepas pin 12, 13, 14 dari fungsi JTAG default!
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    #if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
      // Core v3.x API
      ledcAttach(RED_PIN, 5000, 8);
      ledcAttach(GREEN_PIN, 5000, 8);
      ledcAttach(BLUE_PIN, 5000, 8);
    #else
      // Core v2.x API
      ledcSetup(0, 5000, 8); // Channel 0, 5kHz, 8-bit
      ledcSetup(1, 5000, 8); // Channel 1, 5kHz, 8-bit
      ledcSetup(2, 5000, 8); // Channel 2, 5kHz, 8-bit
      ledcAttachPin(RED_PIN, 0);
      ledcAttachPin(GREEN_PIN, 1);
      ledcAttachPin(BLUE_PIN, 2);
    #endif
    
    // Matikan LED pas pertama kali hidup
    setRGBColor(0, 0, 0);
  #endif

  dht.begin();
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// Fungsi membaca jarak HC-SR04 secara non-blocking
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Timeout 30000 microdetik (sekitar 5 meter maks)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (duration == 0) return -1; // Deteksi error/out of range
  
  return duration * 0.0343 / 2.0;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastPublish >= interval) {
    lastPublish = now;

    // 1. Baca Sensor DHT11
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t) && !isnan(h)) {
      String dht_payload = "{\"temp\":" + String(t, 1) + ",\"humi\":" + String(h, 0) + "}";
      client.publish(topic_dht, dht_payload.c_str());
    } else {
      logToWeb("[WARNING] Gagal membaca DHT11!");
    }

    // 2. Baca HC-SR04
    float dist = getDistance();
    if (dist >= 0) {
      String dist_payload = String(dist, 1);
      client.publish(topic_dist, dist_payload.c_str());
    }

    // 3. Baca Telemetry / Health System ESP32-S3
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    long rssi = WiFi.RSSI();
    uint32_t cpuFreq = ESP.getCpuFreqMHz();
    uint32_t uptime = millis() / 1000;
    
    // Membaca suhu internal core ESP32-S3 (jika didukung compiler core Anda)
    float coreTemp = 0;
    #ifdef SOC_TEMP_SENSOR_SUPPORTED
       // Untuk platform IDF terbaru, menggunakan pembacaan core internal
       coreTemp = temperatureRead();
    #else
       // Fallback jika tidak terdeteksi langsung oleh preprocessor
       coreTemp = temperatureRead(); 
    #endif

    // Format telemetry JSON
    String tele_payload = "{"
                          "\"free_heap\":" + String(freeHeap) + ","
                          "\"total_heap\":" + String(totalHeap) + ","
                          "\"rssi\":" + String(rssi) + ","
                          "\"cpu\":" + String(cpuFreq) + ","
                          "\"uptime\":" + String(uptime) + ","
                          "\"core_temp\":" + String(coreTemp, 1) +
                          "}";
    client.publish(topic_tele, tele_payload.c_str());
  }
}