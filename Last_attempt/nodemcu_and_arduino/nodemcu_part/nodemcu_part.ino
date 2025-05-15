#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// WiFi Configuration
// #define WIFI_SSID "Deathstar"
// #define WIFI_PASSWORD "@submarine"
#define WIFI_SSID "TABISH"
#define WIFI_PASSWORD "TabTab1211"

// Firebase Configuration
#define FIREBASE_HOST "rfid-wifi-keypad-doorlock-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_API_KEY "AIzaSyBDXAiC2eljyHbdVx4NtKJsw0edsU55VeY"

// NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // UTC+5:30 (19800 seconds)

// Hardware Configuration
#define RST_PIN D3    // GPIO0
#define SS_PIN D8     // GPIO15
#define ARDUINO_TX D2 // GPIO5
#define ARDUINO_RX D1 // GPIO4

// Global Objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
MFRC522 mfrc522(SS_PIN, RST_PIN);
SoftwareSerial arduinoSerial(ARDUINO_RX, ARDUINO_TX);

// State Management
bool wifiConnected = false;
bool firebaseInitialized = false;
bool internetAvailable = false;
bool timeSynced = false;
unsigned long lastRfidCheck = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastInternetCheck = 0;

// Timing Constants
const unsigned long RFID_CHECK_INTERVAL = 100;  // 100ms for fast RFID checks
const unsigned long RECONNECT_INTERVAL = 5000;  // 5 seconds
const unsigned long INTERNET_CHECK_INTERVAL = 3000; // 10 seconds

void setup() {
  Serial.begin(9600);
  arduinoSerial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_48dB); // Increase RFID sensitivity

  // Initial connection sequence
  wifiConnected = connectWiFi();
  if(wifiConnected) {
    internetAvailable = checkInternetConnection();
    if(internetAvailable) {
      initializeFirebase();
      initializeTime();
    }
  }
}

void loop() {
  checkRFID();
}

// ----------------------
// Connection Management
// ----------------------
bool connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("\n🔗 Connecting to WiFi");
  
  unsigned long startTime = millis();
  while(millis() - startTime < 20000) {
    checkRFID();
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ WiFi Connected | IP: " + WiFi.localIP().toString());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n❌ WiFi Connection Failed!");
  return false;
}

bool checkInternetConnection() {
  WiFiClient client;
  const char* hosts[] = {"8.8.8.8", "1.1.1.1", "www.google.com"}; // Multiple check hosts
  for(int i = 0; i < 3; i++) {
    checkRFID();
    if(client.connect(hosts[i], 80)) {
      client.stop();
      Serial.println("🌐 Internet Connection Verified");
      return true;
    }
    delay(200);
  }
  Serial.println("🌐 No Internet Connection");
  return false;
}

void maintainConnections() {
  // WiFi Maintenance
  if(WiFi.status() != WL_CONNECTED) {
    if(wifiConnected) {
      Serial.println("\n📵 WiFi Connection Lost!");
      wifiConnected = false;
      internetAvailable = false;
      firebaseInitialized = false;
      timeSynced = false;
    }
    
    if(millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      lastReconnectAttempt = millis();
      Serial.println("\n🔄 Attempting to reconnect to WiFi...");
      WiFi.reconnect();
    }
  } else {
    if(!wifiConnected) {
      wifiConnected = true;
      Serial.println("\n✅ WiFi Reconnected!");
      internetAvailable = checkInternetConnection();
      if(internetAvailable) {
        initializeFirebase();
        initializeTime();
      }
    }
    
    // Periodic internet checks
    if(millis() - lastInternetCheck >= INTERNET_CHECK_INTERVAL) {
      lastInternetCheck = millis();
      bool currentInternet = checkInternetConnection();
      if(currentInternet != internetAvailable) {
        internetAvailable = currentInternet;
        if(internetAvailable) {
          Serial.println("🔄 Internet restored");
          initializeFirebase();
          initializeTime();
        }
      }
    }
  }
}

// -------------------
// Time Management
// -------------------
void initializeTime() {
  if (!internetAvailable) return;

  timeClient.begin();
  timeClient.forceUpdate();  // Force time sync

  if (timeClient.isTimeSet()) {
    timeSynced = true;
    Serial.println("🕒 Time Synced: " + getFormattedTime());
  } else {
    Serial.println("⚠️ Time Sync Failed!");
  }
}

void handleTimeSync() {
  if(internetAvailable) {
    timeClient.update();
    if(!timeSynced && timeClient.isTimeSet()) {
      timeSynced = true;
      Serial.println("🕒 Time Synced: " + getFormattedTime());
    }
  }
}

String getFormattedTime() {
  if(timeSynced) {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    char buffer[30];
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", ptm);
    return String(buffer);
  }
  return "Time not synced";
}

// -------------------
// Firebase Management
// -------------------
bool initializeFirebase() {
  if(!wifiConnected || !internetAvailable) return false;

  config.host = FIREBASE_HOST;
  config.api_key = FIREBASE_API_KEY;

  auth.user.email = "auvdoorlock@zhcet.com";
  auth.user.password = "open-door";

  Serial.println("🔥 Initializing Firebase...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  firebaseInitialized = Firebase.ready();
  if(firebaseInitialized) {
    Serial.println("✅ Firebase Initialized");
    return true;
  } else {
    Serial.println("❌ Firebase Connection Failed: " + fbdo.errorReason());
    return false;
  }
}

// -------------------
// RFID Handling
// -------------------
void checkRFID() {
  if(millis() - lastRfidCheck >= RFID_CHECK_INTERVAL) {
    lastRfidCheck = millis();
    
    if(!mfrc522.PICC_IsNewCardPresent()) return;
    if(!mfrc522.PICC_ReadCardSerial()) return;

    String uid = getUID();
    Serial.print("\n🆔 RFID Detected: ");
    Serial.println(uid);

    bool authorized = isAuthorized(uid);
    handleAccess(authorized);
    maintainConnections();
    handleTimeSync();
    logAccess(uid, authorized);
    
    mfrc522.PICC_HaltA();
  }
}

String getUID() {
  String uid;
  for(byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  return uid;
}

bool isAuthorized(String uid) {
  const String authorizedUIDs[] = {"d39e85fc", "536758fc", "63b795fc", "63473ffc"};
  for(const String& validUID : authorizedUIDs) {
    if(uid.equalsIgnoreCase(validUID)) return true;
  }
  return false;
}

// -------------------
// Access Control
// -------------------
void handleAccess(bool granted) {
  if(granted) {
    Serial.println("✅ Access Granted");
    arduinoSerial.write('O');
  } else {
    Serial.println("❌ Access Denied");
  }
  arduinoSerial.flush();
}

// -------------------
// Logging System
// -------------------
void logAccess(String uid, bool granted) {
  if (!wifiConnected || !internetAvailable) {
    Serial.println("⚠️ No Internet - Skipping Firebase Logging");
    return;
  }

  if(Firebase.isTokenExpired()) {
    Firebase.refreshToken(&config);
  }

  if(!Firebase.ready() && !initializeFirebase()) {
    Serial.println("⚠️ Local Access Only - Cloud Logging Failed");
    return;
  }

  String path = "logs/";
  if(timeSynced) {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    char datePath[20];
    strftime(datePath, 20, "%Y-%m-%d", ptm);
    path += String(datePath) + "/";
    strftime(datePath, 20, "%H-%M-%S", ptm);
    path += String(datePath);
  } else {
    path += "unsynced/" + String(millis());
  }

  FirebaseJson json;
  json.add("uid", uid);
  json.add("access", granted);
  json.add("timestamp", getFormattedTime());

  Serial.println("📁 Attempting Cloud Log: " + path);
  if(Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
    Serial.println("📤 Cloud Log Successful");
  } else {
    Serial.println("❌ Cloud Log Failed: " + fbdo.errorReason());
  }
}