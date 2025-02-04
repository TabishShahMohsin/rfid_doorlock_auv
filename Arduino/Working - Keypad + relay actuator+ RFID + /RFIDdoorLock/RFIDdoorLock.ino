/***************************************************************
  ARDUINO UNO + RC522 RFID + 4×4 KEYPAD + SINGLE RELAY + ACTUATOR
  
  Behavior:
   - Always check RFID or keypad passcode.
   - Keypad passcode flow:
       1) Press 'A' (Start) -> begin or clear passcode entry.
       2) Enter digits.
       3) Press 'B' (Stop) -> check passcode. If correct -> door opens 5s; else remain closed.
       4) Infinite tries; user can press 'A' again to retry.
   - RFID: If an authorized card is detected, door opens 5s immediately.
   - After 5s, door auto-closes (actuator unpowered).
   - Print all statuses to Serial Monitor.
 ***************************************************************/

#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>

// ------------------- RFID Settings -------------------
#define RST_PIN 8  // RST pin to RC522
#define SS_PIN 10  // SDA/SS pin to RC522

MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- Authorized RFID Cards (4-byte UIDs) ---
// Below are the four new cards you requested:
byte authorizedRFID[][4] = {
  { 0xD3, 0x9E, 0x85, 0xFC },
  { 0x53, 0x67, 0x58, 0xFC },
  { 0x63, 0xB7, 0x95, 0xFC },
  { 0x63, 0x47, 0x3F, 0xFC }
};

const int NUM_AUTHORIZED = sizeof(authorizedRFID) / sizeof(authorizedRFID[0]);

// ------------------- Keypad Settings -------------------
const byte ROWS = 4;
const byte COLS = 4;

// Key layout with 'A' = Start, 'B' = Stop
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },  // 'A' = Start
  { '4', '5', '6', 'B' },  // 'B' = Stop
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

// Wiring for the keypad rows/columns
byte rowPins[ROWS] = { A1, A0, 7, 6 };  // R1->2, R2->3, R3->4, R4->5
byte colPins[COLS] = { 5, 4, 3, 2 };    // C1->6, C2->7, C3->14(A0), C4->15(A1)

// Create the Keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ------------------- Relay / Door Control -------------------
#define RELAY_PIN A2           // Using A2 (digital pin 16 on UNO) to drive the relay
bool relayIsActiveLow = true;  // Many relay modules are active LOW

// How long the door stays open (milliseconds)
unsigned long doorOpenTime = 5000;  // 5 seconds
bool doorIsOpen = false;
unsigned long doorTimer = 0;

// ------------------- Passcode Settings -------------------
String correctPin = "1234";  // The valid passcode
String enteredPin = "";

// Only collect digits after "Start" has been pressed
bool passEntryActive = false;

// For optional idle status messages
unsigned long lastStatusPrint = 0;
unsigned long statusPrintInterval = 2000;  // Print idle message every 2 seconds

void setup() {
  Serial.begin(9600);
  while (!Serial) { /* wait for Serial if needed */
  }

  Serial.println("=== System Initializing ===");

  // Initialize RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("RC522 RFID initialized.");

  // Keypad initialization
  Serial.println("Keypad initialized.");

  // Initialize Relay Pin
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);  // Ensure relay OFF at startup
  Serial.println("Relay initialized and set to OFF.");

  Serial.println("=== Setup Complete. System Ready. ===");
}

void loop() {
  // 1. Check RFID
  checkRFID();

  // 2. Check Keypad
  checkKeypad();

  // 3. Manage door auto-close
  if (doorIsOpen) {
    if (millis() - doorTimer >= doorOpenTime) {
      Serial.println("Door open time elapsed. Closing door...");
      closeDoor();
    }
  } else {
    // Print periodic idle status
    if (millis() - lastStatusPrint >= statusPrintInterval) {
      Serial.println("System idle. Door closed. Waiting for RFID or passcode...");
      lastStatusPrint = millis();
    }
  }
}

/********************************************************
   checkRFID():
   - Detects and reads a new RFID card.
   - If recognized, open door for 5s immediately.
 ********************************************************/
void checkRFID() {
  // Look for new card
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;  // No new card
  }
  // Select the card
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;  // Card present but error reading
  }

  Serial.println("RFID card detected!");

  // Compare with authorized list
  if (isAuthorizedCard(mfrc522.uid.uidByte, mfrc522.uid.size)) {
    Serial.println("RFID authorized. Opening door...");
    openDoor();
  } else {
    Serial.println("RFID NOT authorized. Door remains closed.");
  }

  // Halt reading
  mfrc522.PICC_HaltA();
}

/********************************************************
   isAuthorizedCard(uid, size):
   - Checks if read UID matches any in the authorized list
 ********************************************************/
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

/********************************************************
   checkKeypad():
   - Reads keypad input:
     'A' (Start) -> Begin/clear passcode entry
     'B' (Stop)  -> Check passcode if in pass-entry mode
     Digits/others -> Appended to passcode only if in pass-entry mode
 ********************************************************/
void checkKeypad() {
  char key = keypad.getKey();
  if (!key) return;  // No key pressed

  Serial.print("Key pressed: ");
  Serial.println(key);

  switch (key) {
    case '*':
      // 'Start' => begin or reset passcode entry
      passEntryActive = true;
      enteredPin = "";
      Serial.println("Passcode entry begun (or cleared). Enter digits, then press 'B' (Stop) to check.");
      break;

    case '#':
      // 'Stop' => if we're in pass-entry mode, check the passcode
      if (passEntryActive) {
        Serial.print("Stop pressed. Verifying passcode: ");
        Serial.println(enteredPin);

        if (enteredPin.equals(correctPin)) {
          Serial.println("Passcode correct. Opening door...");
          openDoor();
        } else {
          Serial.println("Passcode incorrect. Door remains closed.");
        }
        // End pass-entry attempt (unlimited tries possible by pressing 'A' again)
        passEntryActive = false;
        enteredPin = "";
      } else {
        Serial.println("Stop pressed, but passcode entry was not active. No action taken.");
      }
      break;

    default:
      // If it's a digit or other character, add it if passEntryActive
      if (passEntryActive) {
        enteredPin += key;
        Serial.print("Current passcode buffer: ");
        Serial.println(enteredPin);
      } else {
        Serial.println("Ignoring keypad input because passcode entry is not active. Press 'A' first.");
      }
      break;
  }
}

/********************************************************
   openDoor():
   - Energizes relay -> latch opens
 ********************************************************/
void openDoor() {
  if (!doorIsOpen) {
    setRelay(true);
    doorIsOpen = true;
    doorTimer = millis();
    Serial.println("Door is now OPEN (actuator powered).");
  } else {
    Serial.println("Door is already open. No action needed.");
  }
}

/********************************************************
   closeDoor():
   - De-energizes relay -> latch closes (spring return)
 ********************************************************/
void closeDoor() {
  if (doorIsOpen) {
    setRelay(false);
    doorIsOpen = false;
    Serial.println("Door is now CLOSED (actuator unpowered).");
  } else {
    Serial.println("Door is already closed. No action needed.");
  }
}

/********************************************************
   setRelay(bool on):
   - Controls the relay pin, accounting for active LOW
 ********************************************************/
void setRelay(bool on) {
  if (relayIsActiveLow) {
    digitalWrite(RELAY_PIN, (on ? LOW : HIGH));
  } else {
    digitalWrite(RELAY_PIN, (on ? HIGH : LOW));
  }
  Serial.print("Relay set to: ");
  Serial.println(on ? "ON (energized)" : "OFF (de-energized)");
}
