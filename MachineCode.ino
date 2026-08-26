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
String admin = "123123";

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

bool deductMode = false;
bool deductAmountEntered = false;
bool rechargeMode = false;
bool rechargeAmountEntered = false;
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

  Serial.println("Arduino Credit Card Machine");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Arduino Credit");
  lcd.setCursor(0, 1);
  lcd.print("Card Machine");
  delay(2000);

  showIdleScreen();
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

  if (!deductMode) {
    checkIdleInput();
    return;
  }

  enterDeductAmount();
  if (!deductAmountEntered) return; // don't check for a card until an amount is set

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uID = getUIDString(rfid); // used only for comparison below, never printed

  Serial.println("Card scanned.");

  clearLine(1);
  clearLine(2);
  clearLine(3);
  lcd.setCursor(0, 1);
  lcd.print("Card Scanned");

  if (uID == target) {
    delay(1500);
    processTransaction();
  } else {
    Serial.println("Error: Card Mismatch");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Card Mismatch");
    delay(3000);
  }

  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  deductMode = false;
  deductAmountEntered = false;
  amountStr = "";
  digitIndex = 0;

  showIdleScreen();
}

// Base state: waiting for the operator to choose Pay, Recharge, or Add Card
void checkIdleInput() {
  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key == 'C') {
    deductMode = true;
    amountStr = "";
    digitIndex = 0;

    Serial.println("Pay Mode: Enter Amount");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Amount:");
  } else if (key == 'D') {
    rechargeMode = true;
    amountStr = "";
    digitIndex = 0;

    Serial.println("Recharge Mode: Enter Amount");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Recharge $");
  } else if (key == 'A') {
    registerMode = true;
    registerAdminVerified = false;

    Serial.println("Add Card Mode: Scan Admin Card");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Add Card:");
    lcd.setCursor(0, 1);
    lcd.print("Scan Admin Card");
  } else {
    Serial.println("Invalid key - press C to pay, D to recharge, A to add card");
    lcd.setCursor(0, 3);
    lcd.print("Invalid Key!");
  }
}

void enterRechargeAmount() {
  if (rechargeAmountEntered) return;

  lcd.setCursor(0, 2);
  lcd.print("Digit ");
  lcd.print(digitIndex);
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

      Serial.print("Recharge amount entered: $");
      Serial.println(rechargeAmount, 2);

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
    Serial.println("Recharge cancelled, returning to idle");

    rechargeMode = false;
    rechargeAmountEntered = false;
    amountStr = "";
    digitIndex = 0;

    showIdleScreen();
  } else {
    Serial.println("Invalid, please enter a number.");
    lcd.setCursor(0, 3);
    lcd.print("Enter a number!");
  }
}

void enterDeductAmount() {
  if (deductAmountEntered) return;

  lcd.setCursor(0, 2);
  lcd.print("Digit ");
  lcd.print(digitIndex);
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

      Serial.print("Amount entered: $");
      Serial.println(deductAmount, 2);

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
  } else if (key == '#') {
    Serial.println("Payment cancelled, returning to idle");

    deductMode = false;
    amountStr = "";
    digitIndex = 0;

    showIdleScreen();
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
  Serial.println(currentBalance, 2);

  lcd.setCursor(0, 1);
  lcd.print("Old Balance:");
  lcd.setCursor(0, 2);
  lcd.print(currentBalance, 2);

  if (currentBalance >= deductAmount) {
    float newBalance = currentBalance - deductAmount;
    EEPROM.put(EEPROM_addr, newBalance);

    Serial.print("Transaction Successful! New Balance: $");
    Serial.println(newBalance, 2);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Success!");
    lcd.setCursor(0, 1);
    lcd.print("New Balance: ");
    lcd.setCursor(0, 2);
    lcd.print(newBalance, 2);

    delay(3000);
  } else {
    Serial.println("Error: Not Enough Funds");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Not Enough Funds");

    delay(3000);
  }
}

void checkRecharge() {
  char key = keypad.getKey();
  if (key == '#') {
    Serial.println("Recharge cancelled, returning to idle");

    rechargeMode = false;
    rechargeAmountEntered = false;
    amountStr = "";
    digitIndex = 0;

    showIdleScreen();
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

    Serial.print("Recharged. New Balance: $");
    Serial.println(newBalance, 2);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Recharged!");
    lcd.setCursor(0, 1);
    lcd.print("Balance: ");
    lcd.print(newBalance, 2);

    delay(3000);
  } else {
    Serial.println("Error: Not Admin Card");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Not Admin Card");

    delay(3000);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  rechargeMode = false;
  rechargeAmountEntered = false;
  amountStr = "";
  digitIndex = 0;

  showIdleScreen();
}

void checkRegisterAdmin() {
  char key = keypad.getKey();
  if (key == '#') {
    Serial.println("Add Card cancelled, returning to idle");

    registerMode = false;
    registerAdminVerified = false;

    showIdleScreen();
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String input = getUIDString(rfid);

  if (input == admin) {
    registerAdminVerified = true;

    Serial.println("Admin Verified. Remove admin card, then scan new card");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Admin Verified");
    lcd.setCursor(0, 1);
    lcd.print("Remove, scan new");
  } else {
    Serial.println("Error: Not Admin Card");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Not Admin Card");

    delay(3000);

    registerMode = false;

    showIdleScreen();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void checkRegisterNewCard() {
  char key = keypad.getKey();
  if (key == '#') {
    Serial.println("Add Card cancelled, returning to idle");

    registerMode = false;
    registerAdminVerified = false;

    showIdleScreen();
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String newUID = getUIDString(rfid);

  // Guard against the admin card still lingering on the reader
  if (newUID == admin) {
    Serial.println("Error: Admin card still present - remove it and tap new card");

    lcd.setCursor(0, 2);
    lcd.print("Remove admin");
    lcd.setCursor(0, 3);
    lcd.print("card first!");

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  target = newUID;

  Serial.print("New target card registered: ");
  Serial.println(target);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card Registered!");
  lcd.setCursor(0, 1);
  lcd.print(target);

  delay(3000);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  registerMode = false;
  registerAdminVerified = false;

  showIdleScreen();
}

// Shared idle screen shown whenever the terminal is waiting on the operator
void showIdleScreen() {
  Serial.println("Ready. C: Pay  D: Recharge  A: Add Card");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("C:Pay D:Recharge");
  lcd.setCursor(0, 1);
  lcd.print("A:Add Card");
}