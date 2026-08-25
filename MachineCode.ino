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
float rechargeAmount = 0.0;
float startBalance;
int EEPROM_addr = 0;
String target = "2388101B";
String admin = "89548047";

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

bool deductAmountEntered = false;
bool rechargeAmountEntered = false;
bool rechargeMode = false;
bool registerMode = false;
bool registerAdminVerified = false;
String amountStr = "";
int digitIndex = 0;

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

}

void loop() {  
  if (registerMode) {
    if (!registerAdminVerified) {
      checkRegisterAdmin();
      return;
    }
    checkRegisterNewCard();
    return;
  }

  if (rechargeMode) {
    if (!rechargeAmountEntered) {
      enterRechargeAmount();
      return;
    }
    checkRecharge();
    return;
  }

  enterDeductAmount();
  if (!deductAmountEntered) return; // don't check for a card until an amount is set

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

  deductAmountEntered = false;
  amountStr = "";
  digitIndex = 0;
}

void enterRechargeAmount() {
  if (rechargeAmountEntered) return;

  lcd.setCursor(0, 2);
  lcd.print("Digit");
  lcd.setCursor(7, 2);
  lcd.print(digitIndex);
  lcd.setCursor(8, 2);
  lcd.print(": ");

  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key >= '0' && key <= '9') {
    amountStr += key;
    lcd.setCursor(9, 2);
    lcd.print(key);
    digitIndex++;

    if (digitIndex >= 4) {
      rechargeAmount = amountStr.toInt() / 100.0;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Recharge: $");
      lcd.print(rechargeAmount, 2);
      lcd.setCursor(0, 1);
      lcd.print("Scan Admin Card:");

      rechargeAmountEntered = true;
    } else {
      lcd.clear();
    }
  } else if (key == '*') {
    if (amountStr.length() > 0) {
      amountStr.remove(amountStr.length() - 1);
      digitIndex--;
    }
    lcd.clear();
  } else if (key == '#') {
    rechargeMode = false;
    rechargeAmountEntered = false;
    amountStr = "";
    digitIndex = 0;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Card: ");
  } else {
    Serial.println("Invalid, please enter a number.");
    lcd.setCursor(0, 3);
    lcd.print("Enter a number!");
  }
}

void enterDeductAmount() {
  if (deductAmountEntered) return;

  lcd.setCursor(0, 2);
  lcd.print("Digit");
  lcd.setCursor(7, 2);
  lcd.print(digitIndex);
  lcd.setCursor(8, 2);
  lcd.print(": ");

  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key >= '0' && key <= '9') {
    amountStr += key;
    lcd.setCursor(9, 2);
    lcd.print(key);
    digitIndex++;

    if (digitIndex >= 4) {
      deductAmount = amountStr.toInt() / 100.0;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Amount: $");
      lcd.print(deductAmount, 2);
      lcd.setCursor(0, 1);
      lcd.print("Scan Card:");

      deductAmountEntered = true;
    } else {
      lcd.clear();
    }
  } else if (key == '*') {
    if (amountStr.length() > 0) {
      amountStr.remove(amountStr.length() - 1);
      digitIndex--;
    }
    lcd.clear();
  } else if (key == 'D') {
    rechargeMode = true;
    amountStr = "";
    digitIndex = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Recharge $");
  } else if (key == 'A') {
    registerMode = true;
    registerAdminVerified = false;
    amountStr = "";
    digitIndex = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Add Card:");
    lcd.setCursor(0, 1);
    lcd.print("Scan Admin Card");
  } else {
    Serial.println("Invalid, please enter a number.");
    lcd.setCursor(0, 3);
    lcd.print("Enter a number!");
  }
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

void checkRecharge() {
  char key = keypad.getKey();
  if (key == '#') {
    rechargeMode = false;
    rechargeAmountEntered = false;
    amountStr = "";
    digitIndex = 0;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Card: ");
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String input = getUIDString(rfid);

  if (input == admin) {
    float currentBalance = 0.0;
    EEPROM.get(EEPROM_addr, currentBalance);
    float newBalance = currentBalance + rechargeAmount;
    EEPROM.put(EEPROM_addr, newBalance);

    Serial.print("Recharged. New Balance: ");
    Serial.println(newBalance);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Recharged!");
    lcd.setCursor(0, 1);
    lcd.print("Balance: ");
    lcd.print(newBalance, 2);
    delay(3000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Not Admin Card");

    Serial.println("Error: Not Admin Card");
    delay(3000);
  }

  rechargeMode = false;
  rechargeAmountEntered = false;
  amountStr = "";
  digitIndex = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Card: ");
}

void checkRegisterAdmin() {
  char key = keypad.getKey();
  if (key == '#') {
    registerMode = false;
    registerAdminVerified = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Card: ");
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String input = getUIDString(rfid);

  if (input == admin) {
    registerAdminVerified = true;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Admin Verified");
    lcd.setCursor(0, 1);
    lcd.print("Scan New Card");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Not Admin Card");

    Serial.println("Error: Not Admin Card");
    delay(3000);

    registerMode = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Card: ");
  }
}

void checkRegisterNewCard() {
  char key = keypad.getKey();
  if (key == '#') {
    registerMode = false;
    registerAdminVerified = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Card: ");
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String newUID = getUIDString(rfid);
  target = newUID;

  Serial.print("New target card registered: ");
  Serial.println(target);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card Registered!");
  lcd.setCursor(0, 1);
  lcd.print(target);
  delay(3000);

  registerMode = false;
  registerAdminVerified = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Card: ");
}