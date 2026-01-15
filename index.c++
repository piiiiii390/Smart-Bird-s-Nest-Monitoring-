#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 4
#define DHTTYPE DHT22
#define RELAY_SUHU 26  
#define RELAY_AIR 25    
#define BUZZER_PIN 27
#define TRIG_PIN 18
#define ECHO_PIN 19

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); 
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";

unsigned long lastBeep = 0;
bool beepState = false;

// Variabel kontrol Dashboard
bool manualKipasSuhu = false;
bool manualKipasAir = false;

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // Membersihkan spasi yang mungkin terbawa dari Node-RED
  message.trim(); 

  // LOGIKA PENERIMAAN DATA DARI NODE-RED
  if (String(topic) == "walet/kipas_suhu") {
    if (message == "ON") manualKipasSuhu = true;
    else if (message == "OFF") manualKipasSuhu = false;
  }
  
  if (String(topic) == "walet/kipas_air") {
    if (message == "ON") manualKipasAir = true;
    else if (message == "OFF") manualKipasAir = false;
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Walet_Final_Sopia")) {
      client.subscribe("walet/kipas_suhu");
      client.subscribe("walet/kipas_air");
    } else { delay(2000); }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_SUHU, OUTPUT);
  pinMode(RELAY_AIR, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT); 

  lcd.init(); 
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Starting");
  
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  ledcAttach(BUZZER_PIN, 2000, 8); 
  lcd.clear();
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  float suhu = dht.readTemperature();
  float hum = dht.readHumidity();
  
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  long jarak = duration * 0.034 / 2;

  // LOGIKA RELAY (Otomatis SENSOR || Manual TOMBOL)
  if (suhu > 30.0 || manualKipasSuhu) {
    digitalWrite(RELAY_SUHU, HIGH);
  } else {
    digitalWrite(RELAY_SUHU, LOW);
  }

  if (hum < 60.0 || manualKipasAir) {
    digitalWrite(RELAY_AIR, HIGH);
  } else {
    digitalWrite(RELAY_AIR, LOW);
  }

  // LOGIKA BUZZER
  bool suhuPanas = (suhu > 32);
  bool humRendah = (hum < 60);
  bool airHabis  = (jarak > 80 && jarak < 400);

  if (suhuPanas || humRendah || airHabis) {
    int nada, jeda;
    if (suhuPanas) { nada = 3500; jeda = 150; } 
    else if (humRendah) { nada = 2000; jeda = 500; } 
    else { nada = 1000; jeda = 1000; }

    if (millis() - lastBeep >= jeda) {
      lastBeep = millis();
      beepState = !beepState;
      ledcWriteTone(BUZZER_PIN, beepState ? nada : 0);
    }
  } else {
    ledcWriteTone(BUZZER_PIN, 0);
  }

  // Update LCD
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(suhu, 1);
  lcd.print("C H:"); lcd.print(hum, 0); lcd.print("% ");
  
  lcd.setCursor(0, 1);
  if(suhuPanas) lcd.print("ALARM: PANAS!  ");
  else if(humRendah) lcd.print("ALARM: KERING! ");
  else if(airHabis) lcd.print("ALARM: AIR LOW ");
  else lcd.print("Sistem: Aman    ");

  // MENAMPILKAN SATUAN CM DI SERIAL MONITOR (Penting untuk laporan)
  Serial.print("Suhu: "); Serial.print(suhu);
  Serial.print(" | Hum: "); Serial.print(hum);
  Serial.print(" | Jarak: "); Serial.print(jarak); Serial.println(" cm");

  // Publish Data ke Node-RED
  client.publish("walet/suhu", String(suhu).c_str());
  client.publish("walet/kelembapan", String(hum).c_str());
  client.publish("walet/jarak", String(jarak).c_str());

  delay(200); 
}