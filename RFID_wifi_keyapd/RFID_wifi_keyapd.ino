/***************************************************************
  ESP32 + RC522 RFID + 4x4 KEYPAD + SINGLE RELAY + ACTUATOR 
  
  Behavior:
   - Always check RFID or keypad passcode.
   - Keypad passcode flow:
       1) Press Start -> begin or clear passcode entry.
       2) Enter digits.
       3) Press Stop -> check passcode. If correct -> door opens 5s; else remain closed.
       4) The password entered will be shown at the database even if wrong. -- Added time and date.
       5) Infinite tries; user can press Start again to retry.
   - RFID:
      1) If an authorized card is detected, door opens for 5s immediately.
      2) The RFID will be uploaded to the database even if wrong.
   - Print all statuses to Serial Monitor.
 ***************************************************************/

#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Wire.h>
#include "time.h"

// WiFi Settings
#define WIFI_SSID "Deathstar"
#define WIFI_PASSWORD "@submarine"
#define API_KEY "<YOUR_FIREBASE_API_KEY>"
#define DATABASE_URL "<YOUR_FIREBASE_DATABASE_URL>"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
char _RFID[12] = "";
char _KeyEntered[7] = "000000";

// RFID Settings
#define RST_PIN 22
#define SS_PIN 21
MFRC522 mfrc522(SS_PIN, RST_PIN);

byte authorizedRFID[][4] = {
  { 0xD3, 0x9E, 0x85, 0xFC },
  { 0x53, 0x67, 0x58, 0xFC }
};
const int NUM_AUTHORIZED = sizeof(authorizedRFID) / sizeof(authorizedRFID[0]);

// Keypad Settings
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 19, 18, 5, 17 };
byte colPins[COLS] = { 16, 4, 0, 2 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Relay Control
#define RELAY_PIN 15
bool relayIsActiveLow = true;
unsigned long doorOpenTime = 5000;
bool doorIsOpen = false;
unsigned long doorTimer = 0;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  SPI.begin();
  mfrc522.PCD_Init();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, relayIsActiveLow ? HIGH : LOW);
}

void loop() {
  checkRFID();
  checkKeypad();
  if (doorIsOpen && millis() - doorTimer >= doorOpenTime) {
    closeDoor();
  }
}

void checkRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;
  Serial.println("RFID card detected!");

  if (isAuthorizedCard(mfrc522.uid.uidByte, mfrc522.uid.size)) {
    Serial.println("RFID authorized. Opening door...");
    openDoor();
  } else {
    Serial.println("Unauthorized RFID detected.");
  }
  mfrc522.PICC_HaltA();
}

bool isAuthorizedCard(byte *uid, byte uidSize) {
  for (int i = 0; i < NUM_AUTHORIZED; i++) {
    bool match = true;
    for (int j = 0; j < uidSize; j++) {
      if (authorizedRFID[i][j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

void checkKeypad() {
  char key = keypad.getKey();
  if (!key) return;
  Serial.print("Key pressed: ");
  Serial.println(key);

  if (key == '*') {
    strcpy(_KeyEntered, "");
  } else if (key == '#') {
    if (strcmp(_KeyEntered, "1234") == 0) {
      Serial.println("Correct Passcode. Opening door...");
      openDoor();
    } else {
      Serial.println("Wrong passcode. Door remains closed.");
    }
  } else {
    strncat(_KeyEntered, &key, 1);
  }
}

void openDoor() {
  digitalWrite(RELAY_PIN, relayIsActiveLow ? LOW : HIGH);
  doorIsOpen = true;
  doorTimer = millis();
  Serial.println("Door OPENED");
}

void closeDoor() {
  digitalWrite(RELAY_PIN, relayIsActiveLow ? HIGH : LOW);
  doorIsOpen = false;
  Serial.println("Door CLOSED");
}
