#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// RFID Declarations
int rstPin = 9;
int ssPin = 10;
MFRC522 rfid(ssPin, rstPin);

float deductAmount = 5.00;
float startBalance;
int EEPROM_addr = 0;
int resetBalance = false;
String target = "2388101B";

// LCD Declarations
LiquidCrystal_I2C lcd(0x27, 16, 4);

// Keypad Declarations
const byte COLUMNS = 4;
const byte ROWS = 4;

char keys[ROWS][COLUMNS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'},
};

const byte rowPins[ROWS] = {2, 3, 4, 5};
const byte columnPins[COLUMNS] = {6, 7, 8, A0};
Keypad keypad(makeKeymap(keys), rowPins, columnPins, ROWS, COLUMNS);

bool amountEntered = false;

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  Serial.println("Scan Card: ");

  lcd.setCursor(0, 0);
  lcd.print("Scan Card: ");

  if (resetBalance) {
    startBalance = 50.00;
    EEPROM.put(EEPROM_addr, startBalance);
    Serial.print("Balance: ");
    Serial.println(startBalance);

    lcd.setCursor(0, 1);
    lcd.print("Balance: ");
    lcd.print(startBalance);

    Serial.println("Set to false!");
  }
}

void loop() {
  if (!amountEntered) {
    String amountStr = "";

    for (int i = 0; i < 4; i++) {
      bool asking = true;
      while (asking) {
        lcd.setCursor(0, 2);
        lcd.print("Digit");
        lcd.setCursor(7, 2);
        lcd.print(i);
        lcd.setCursor(8, 2);
        lcd.print(": ");

        char key = keypad.getKey();

        if (key != NO_KEY) {
          if (key >= '0' && key <= '9') {
            amountStr += key;
            lcd.setCursor(9, 2);
            lcd.print(key);
            delay(1000);
            asking = false;
          } else {
            Serial.println("Invalid, please enter a number.");
            lcd.setCursor(0, 3);
            lcd.print("Enter a number!");
            delay(1000);
            clearLine(3);
          }
        }
      }
      lcd.clear();
    }

    deductAmount = amountStr.toInt() / 100.0;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Amount: $");
    lcd.print(deductAmount, 2);
    lcd.setCursor(0, 1);
    lcd.print("Scan Card:");

    amountEntered = true;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uID = getUIDString(rfid);
  Serial.print("Scanned Card: ");
  clearLine(1);
  clearLine(2);
  clearLine(3);
  lcd.setCursor(0, 1);
  lcd.print("Scanned: ");

  Serial.println(uID);
  lcd.setCursor(0, 2);
  lcd.print(uID);

  if (uID == target) {
    delay(1500);
    processTransaction();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Card Mismatch");

    Serial.println("Error: Card Mismatch");
    delay(3000);
  }

  Serial.println();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Card: ");

  amountEntered = false;
}

void clearLine(int row) {
  lcd.setCursor(0, row);
  lcd.print("                ");
}

String getUIDString(MFRC522 &reader) {
  String uid = "";
  for (int i = 0; i < reader.uid.size; i++) {
    if (reader.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(reader.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void processTransaction() {
  lcd.clear();
  float currentBalance = 0.0;
  EEPROM.get(EEPROM_addr, currentBalance);

  Serial.print("Current Balance: $");
  Serial.print(currentBalance);

  clearLine(1);
  lcd.setCursor(0, 1);
  lcd.print("Current Balance: ");
  lcd.setCursor(0, 2);
  lcd.print(currentBalance, 2);

  if (currentBalance >= deductAmount) {
    float newBalance = currentBalance - deductAmount;
    Serial.println("Transaction Successful!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Success!");

    Serial.print("New Balance: ");
    Serial.println(newBalance);

    lcd.setCursor(0, 1);
    lcd.print("New Balance: ");
    lcd.setCursor(0, 2);
    lcd.print(newBalance, 2);
    EEPROM.put(EEPROM_addr, newBalance);

    delay(3000);
  } else {
    Serial.println("Error: Insufficient Funds");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Insufficient Funds");

    delay(3000);
  }
}