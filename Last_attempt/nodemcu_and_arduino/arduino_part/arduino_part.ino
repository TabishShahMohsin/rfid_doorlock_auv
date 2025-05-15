#include <SoftwareSerial.h>
#include <Keypad.h>

// Keypad Configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {0, 1, 2, 3};
byte colPins[COLS] = {4, 5, 6, 7};
// byte rowPins[ROWS] = {2, 3, 4, 5};
// byte colPins[COLS] = {6, 7, 8, 9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Relay Configuration
#define RELAY_PIN 11
bool relayActiveLow = true;
unsigned long doorTimeout = 5000;
bool doorOpen = false;
unsigned long doorTimer = 0;

// Security
String masterPIN = "123";
String inputPIN = "";
bool pinEntryMode = false;

// NodeMCU Communication
SoftwareSerial nodeMCU(12, 11); // RX=2, TX=3

void setup() {
  Serial.begin(9600);
  nodeMCU.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);
  Serial.println("System Ready");
}

void loop() {
  delay(50);
  checkSerial();
  checkKeypad();
  manageDoor();
}

void checkSerial() {
  if (nodeMCU.available()) {
    char command = nodeMCU.read();
    if (command > 0) { // Ensure valid character received
      Serial.print("Received command: ");
      Serial.println(command);

      if (command == 'O') {
        Serial.println("✅ RFID Authorization - Opening Door");
        activateDoor();
      }
    }
  }
}

void checkKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  Serial.print("Key Pressed: ");
  Serial.println(key);

  switch (key) {
    case '*':
      pinEntryMode = true;
      inputPIN = "";
      Serial.println("Enter PIN (Press STOP to submit)");
      break;
    case '#':
      if (pinEntryMode) {
        if (inputPIN == masterPIN) {
          Serial.println("Valid PIN - Opening Door");
          activateDoor();
        } else {
          Serial.println("Invalid PIN");
        }
        pinEntryMode = false;
        inputPIN = "";
      }
      break;
    default:
      if (pinEntryMode && isdigit(key)) {
        inputPIN += key;
        Serial.print("Current Input: ");
        Serial.println(inputPIN);
      }
      break;
  }
}

void manageDoor() {
  if (doorOpen && millis() - doorTimer >= doorTimeout) {
    Serial.println("Auto-Closing Door");
    setRelay(false);
    doorOpen = false;
  }
}

void activateDoor() {
  if (!doorOpen) {
    Serial.println("Opening Door");
    setRelay(true);
    doorOpen = true;
    doorTimer = millis();
  }
}

void setRelay(bool state) {
  digitalWrite(RELAY_PIN, relayActiveLow ? !state : state);
}