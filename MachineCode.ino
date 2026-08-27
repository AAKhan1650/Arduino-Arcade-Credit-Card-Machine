#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// RFID Declarations
int rstPin = 9;
int ssPin = 10;
MFRC522 rfid(ssPin, rstPin);

float deductAmount;
float rechargeAmount;
String admin = "89548047";

// Four independent users, each with their own UID and balance
String userOneUID = "2388101B";
String userTwoUID = "";
String userThreeUID = "";
String userFourUID = "";

float userOneBalance = 0.0;
float userTwoBalance = 0.0;
float userThreeBalance = 0.0;
float userFourBalance = 0.0;

// EEPROM addresses: 4 balances, then 4 UIDs, spaced out so they never overlap
const int BAL1_ADDR = 0;
const int BAL2_ADDR = 4;
const int BAL3_ADDR = 8;
const int BAL4_ADDR = 12;
const int UID1_ADDR = 20;
const int UID2_ADDR = 40;
const int UID3_ADDR = 60;
const int UID4_ADDR = 80;
const int UID_MAX_LEN = 20;

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

// Mode flags track which "screen" the terminal is currently on
bool deductMode = false;
bool deductAmountEntered = false;

bool rechargeMode = false;
bool rechargeAmountEntered = false;
bool rechargeAdminVerified = false;
int rechargeSlot = 0; // which user (1-4) is being recharged, 0 = not chosen yet
bool rechargeMenuShown = false; // only draw the user-select menu once

bool registerMode = false;
bool registerAdminVerified = false;
int registerSlot = 0; // which user slot (1-4) is being registered, 0 = not chosen yet
bool registerMenuShown = false; // only draw the user-select menu once

// Amount entry is now a plain number, not a String, to avoid memory fragmentation
long amountBufferCents = 0;
int digitIndex = 0;

void clearLine(int row) {
  lcd.setCursor(0, row);
  lcd.print(F("                "));
}

void showIdleScreen() {
  Serial.println(F("Ready. A: Add Card  C: Pay  D: Recharge"));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("A:Add Card"));
  lcd.setCursor(0, 1);
  lcd.print(F("C:Pay"));
  lcd.setCursor(0, 2);
  lcd.print(F("D:Recharge"));
}

void saveUIDToEEPROM(int addr, String uid) {
  int len = uid.length();
  if (len > UID_MAX_LEN - 1) len = UID_MAX_LEN - 1;

  for (int i = 0; i < len; i++) {
    EEPROM.update(addr + i, uid[i]);
  }
  EEPROM.update(addr + len, '\0');
}

String loadUIDFromEEPROM(int addr) {
  char buf[UID_MAX_LEN];
  int i = 0;

  for (; i < UID_MAX_LEN - 1; i++) {
    char c = EEPROM.read(addr + i);
    if (c == '\0' || (byte)c == 0xFF) break;
    buf[i] = c;
  }
  buf[i] = '\0';

  return String(buf);
}

void setUserUID(int user, String uid) {
  if (user == 1) {
    userOneUID = uid;
    saveUIDToEEPROM(UID1_ADDR, uid);
  } else if (user == 2) {
    userTwoUID = uid;
    saveUIDToEEPROM(UID2_ADDR, uid);
  } else if (user == 3) {
    userThreeUID = uid;
    saveUIDToEEPROM(UID3_ADDR, uid);
  } else {
    userFourUID = uid;
    saveUIDToEEPROM(UID4_ADDR, uid);
  }
}

float getUserBalance(int user) {
  if (user == 1) return userOneBalance;
  if (user == 2) return userTwoBalance;
  if (user == 3) return userThreeBalance;
  return userFourBalance;
}

void setUserBalance(int user, float value) {
  if (user == 1) {
    userOneBalance = value;
    EEPROM.put(BAL1_ADDR, value);
  } else if (user == 2) {
    userTwoBalance = value;
    EEPROM.put(BAL2_ADDR, value);
  } else if (user == 3) {
    userThreeBalance = value;
    EEPROM.put(BAL3_ADDR, value);
  } else {
    userFourBalance = value;
    EEPROM.put(BAL4_ADDR, value);
  }
}

void loadAllFromEEPROM() {
  float b1, b2, b3, b4;
  EEPROM.get(BAL1_ADDR, b1);
  EEPROM.get(BAL2_ADDR, b2);
  EEPROM.get(BAL3_ADDR, b3);
  EEPROM.get(BAL4_ADDR, b4);

  if (isnan(b1)) {
    userOneBalance = 0.00;
  } else {
    userOneBalance = b1;
  }

  if (isnan(b2)) {
    userTwoBalance = 0.00;
  } else {
    userTwoBalance = b2;
  }

  if (isnan(b3)) {
    userThreeBalance = 0.00;
  } else {
    userThreeBalance = b3;
  }

  if (isnan(b4)) {
    userFourBalance = 0.00;
  } else {
    userFourBalance = b4;
  }

  String u1 = loadUIDFromEEPROM(UID1_ADDR);
  String u2 = loadUIDFromEEPROM(UID2_ADDR);
  String u3 = loadUIDFromEEPROM(UID3_ADDR);
  String u4 = loadUIDFromEEPROM(UID4_ADDR);

  if (u1.length() > 0) userOneUID = u1;
  if (u2.length() > 0) userTwoUID = u2;
  if (u3.length() > 0) userThreeUID = u3;
  if (u4.length() > 0) userFourUID = u4;

  Serial.println(F("Loaded saved users and balances from EEPROM."));
}

void resetRechargeState() {
  rechargeMode = false;
  rechargeAmountEntered = false;
  rechargeAdminVerified = false;
  rechargeSlot = 0;
  rechargeMenuShown = false;
  amountBufferCents = 0;
  digitIndex = 0;
}

void resetRegisterState() {
  registerMode = false;
  registerAdminVerified = false;
  registerSlot = 0;
  registerMenuShown = false;
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

void checkRegisterAdmin() {
  char key = keypad.getKey();
  if (key == '#') {
    Serial.println(F("Add Card cancelled, returning to idle"));
    resetRegisterState();
    showIdleScreen();
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String input = getUIDString(rfid);

  if (input == admin) {
    registerAdminVerified = true;

    Serial.println(F("Admin Verified. Select slot for new card."));

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Admin Verified"));
    lcd.setCursor(0, 1);
    lcd.print(F("Select Slot"));
  } else {
    Serial.println(F("Error: Not Admin Card"));

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Error: "));
    lcd.setCursor(0, 1);
    lcd.print(F("Not Admin Card"));

    delay(3000);
    resetRegisterState();
    showIdleScreen();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void selectRegisterSlot() {
  if (!registerMenuShown) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("1: User 1"));
    lcd.setCursor(0, 1);
    lcd.print(F("2: User 2"));
    lcd.setCursor(0, 2);
    lcd.print(F("3: User 3"));
    lcd.setCursor(0, 3);
    lcd.print(F("4: User 4"));
    registerMenuShown = true;
  }

  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key == '#') {
    Serial.println(F("Add Card cancelled, returning to idle"));
    resetRegisterState();
    showIdleScreen();
    return;
  }

  if (key < '1' || key > '4') {
    Serial.println(F("Invalid, choose 1-4"));
    return;
  }

  registerSlot = key - '0';

  Serial.print(F("Selected slot "));
  Serial.println(registerSlot);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Scan New Card"));
  lcd.setCursor(0, 1);
  lcd.print(F("For User "));
  lcd.print(registerSlot);
}

void checkRegisterNewCard() {
  char key = keypad.getKey();
  if (key == '#') {
    Serial.println(F("Add Card cancelled, returning to idle"));
    resetRegisterState();
    showIdleScreen();
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String newUID = getUIDString(rfid);

  if (newUID == admin) {
    Serial.println(F("Error: Admin card still present - remove it and tap new card"));

    lcd.setCursor(0, 2);
    lcd.print(F("Remove admin"));
    lcd.setCursor(0, 3);
    lcd.print(F("card first!"));

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  setUserUID(registerSlot, newUID);

  Serial.print(F("User "));
  Serial.print(registerSlot);
  Serial.print(F(" registered: "));
  Serial.println(newUID);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Registered User "));
  lcd.print(registerSlot);
  lcd.setCursor(0, 1);
  lcd.print(newUID);

  delay(3000);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  resetRegisterState();
  showIdleScreen();
}

void enterRechargeAmount() {
  if (rechargeAmountEntered) return;

  lcd.setCursor(0, 2);
  lcd.print(F("Digit "));
  lcd.print(digitIndex);
  lcd.print(F(": "));

  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key >= '0' && key <= '9') {
    amountBufferCents = amountBufferCents * 10 + (key - '0');
    lcd.setCursor(9, 2);
    lcd.print(key);
    digitIndex++;

    if (digitIndex >= 4) {
      rechargeAmount = amountBufferCents / 100.0;

      Serial.print(F("Recharge amount entered: $"));
      Serial.println(rechargeAmount, 2);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("Recharge: $"));
      lcd.print(rechargeAmount, 2);
      lcd.setCursor(0, 1);
      lcd.print(F("Scan Admin Card:"));

      rechargeAmountEntered = true;
    } else {
      lcd.clear();
    }
  } else if (key == '*') {
    if (digitIndex > 0) {
      amountBufferCents = amountBufferCents / 10;
      digitIndex--;
    }
    lcd.clear();
  } else if (key == '#') {
    Serial.println(F("Recharge cancelled, returning to idle"));
    resetRechargeState();
    showIdleScreen();
  } else {
    Serial.println(F("Invalid, please enter a number."));
    lcd.setCursor(0, 3);
    lcd.print(F("Enter a number!"));
  }
}

void checkRechargeAdmin() {
  char key = keypad.getKey();
  if (key == '#') {
    Serial.println(F("Recharge cancelled, returning to idle"));
    resetRechargeState();
    showIdleScreen();
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String input = getUIDString(rfid);

  if (input == admin) {
    rechargeAdminVerified = true;

    Serial.println(F("Admin Verified. Select user to recharge."));

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Admin Verified"));
    lcd.setCursor(0, 1);
    lcd.print(F("Select User"));
  } else {
    Serial.println(F("Error: Not Admin Card"));

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Error: "));
    lcd.setCursor(0, 1);
    lcd.print(F("Not Admin Card"));

    delay(3000);
    resetRechargeState();
    showIdleScreen();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void selectRechargeSlot() {
  if (!rechargeMenuShown) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("1: User 1"));
    lcd.setCursor(0, 1);
    lcd.print(F("2: User 2"));
    lcd.setCursor(0, 2);
    lcd.print(F("3: User 3"));
    lcd.setCursor(0, 3);
    lcd.print(F("4: User 4"));
    rechargeMenuShown = true;
  }

  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key == '#') {
    Serial.println(F("Recharge cancelled, returning to idle"));
    resetRechargeState();
    showIdleScreen();
    return;
  }

  if (key < '1' || key > '4') {
    Serial.println(F("Invalid, choose 1-4"));
    return;
  }

  rechargeSlot = key - '0';

  Serial.print(F("Selected User "));
  Serial.println(rechargeSlot);
}

void applyRecharge() {
  float currentBalance = getUserBalance(rechargeSlot);
  float newBalance = currentBalance + rechargeAmount;
  setUserBalance(rechargeSlot, newBalance);

  Serial.print(F("User "));
  Serial.print(rechargeSlot);
  Serial.print(F(" Recharged. New Balance: $"));
  Serial.println(newBalance, 2);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Recharged User "));
  lcd.print(rechargeSlot);
  lcd.setCursor(0, 1);
  lcd.print(F("Balance: "));
  lcd.print(newBalance, 2);

  delay(3000);

  resetRechargeState();
  showIdleScreen();
}

void checkIdleInput() {
  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key == 'C') {
    deductMode = true;
    amountBufferCents = 0;
    digitIndex = 0;

    Serial.println(F("Pay Mode: Enter Amount"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Enter Amount:"));
  } else if (key == 'D') {
    rechargeMode = true;
    rechargeAmountEntered = false;
    rechargeAdminVerified = false;
    rechargeSlot = 0;
    rechargeMenuShown = false;
    amountBufferCents = 0;
    digitIndex = 0;

    Serial.println(F("Recharge Mode: Enter Amount"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Enter Recharge $"));
  } else if (key == 'A') {
    registerMode = true;
    registerAdminVerified = false;
    registerSlot = 0;
    registerMenuShown = false;

    Serial.println(F("Add Card Mode: Scan Admin Card"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Add Card:"));
    lcd.setCursor(0, 1);
    lcd.print(F("Scan Admin Card"));
  } else {
    Serial.println(F("Invalid key - press C to pay, D to recharge, A to add card"));
    lcd.setCursor(0, 3);
    lcd.print(F("Invalid Key!"));
  }
}

void enterDeductAmount() {
  if (deductAmountEntered) return;

  lcd.setCursor(0, 2);
  lcd.print(F("Digit "));
  lcd.print(digitIndex);
  lcd.print(F(": "));

  char key = keypad.getKey();
  if (key == NO_KEY) return;

  if (key >= '0' && key <= '9') {
    amountBufferCents = amountBufferCents * 10 + (key - '0');
    lcd.setCursor(9, 2);
    lcd.print(key);
    digitIndex++;

    if (digitIndex >= 4) {
      deductAmount = amountBufferCents / 100.0;

      Serial.print(F("Amount entered: $"));
      Serial.println(deductAmount, 2);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("Amount: $"));
      lcd.print(deductAmount, 2);
      lcd.setCursor(0, 1);
      lcd.print(F("Scan Card:"));

      deductAmountEntered = true;
    } else {
      lcd.clear();
    }
  } else if (key == '*') {
    if (digitIndex > 0) {
      amountBufferCents = amountBufferCents / 10;
      digitIndex--;
    }
    lcd.clear();
  } else if (key == '#') {
    Serial.println(F("Payment cancelled, returning to idle"));

    deductMode = false;
    amountBufferCents = 0;
    digitIndex = 0;

    showIdleScreen();
  } else {
    Serial.println(F("Invalid, please enter a number."));
    lcd.setCursor(0, 3);
    lcd.print(F("Enter a number!"));
  }
}

void processTransaction(int user) {
  lcd.clear();
  float currentBalance = getUserBalance(user);

  Serial.print(F("User "));
  Serial.print(user);
  Serial.print(F(" Current Balance: $"));
  Serial.println(currentBalance, 2);

  lcd.setCursor(0, 1);
  lcd.print(F("Old Balance:"));
  lcd.setCursor(0, 2);
  lcd.print(currentBalance, 2);

  if (currentBalance >= deductAmount) {
    float newBalance = currentBalance - deductAmount;
    setUserBalance(user, newBalance);

    Serial.print(F("Transaction Successful! New Balance: $"));
    Serial.println(newBalance, 2);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Success! User "));
    lcd.print(user);
    lcd.setCursor(0, 1);
    lcd.print(F("New Balance: "));
    lcd.setCursor(0, 2);
    lcd.print(newBalance, 2);

    delay(3000);
  } else {
    Serial.println(F("Error: Not Enough Funds"));

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Error: "));
    lcd.setCursor(0, 1);
    lcd.print(F("Not Enough Funds"));

    delay(3000);
  }
}

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  Serial.println(F("Arduino Credit Card Machine"));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Arduino Credit"));
  lcd.setCursor(0, 1);
  lcd.print(F("Card Machine"));
  delay(2000);

  loadAllFromEEPROM();
  showIdleScreen();
}

void loop() {
  if (registerMode) {
    if (!registerAdminVerified) {
      checkRegisterAdmin();
      return;
    }
    if (registerSlot == 0) {
      selectRegisterSlot();
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
    if (!rechargeAdminVerified) {
      checkRechargeAdmin();
      return;
    }
    if (rechargeSlot == 0) {
      selectRechargeSlot();
      return;
    }
    applyRecharge();
    return;
  }

  if (!deductMode) {
    checkIdleInput();
    return;
  }

  enterDeductAmount();
  if (!deductAmountEntered) return;

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uID = getUIDString(rfid);

  Serial.println(F("Card scanned."));

  clearLine(1);
  clearLine(2);
  clearLine(3);
  lcd.setCursor(0, 1);
  lcd.print(F("Card Scanned"));

  // Figure out which registered user this card belongs to, if any
  int matchedUser = 0;
  if (userOneUID.length() > 0 && uID == userOneUID) matchedUser = 1;
  else if (userTwoUID.length() > 0 && uID == userTwoUID) matchedUser = 2;
  else if (userThreeUID.length() > 0 && uID == userThreeUID) matchedUser = 3;
  else if (userFourUID.length() > 0 && uID == userFourUID) matchedUser = 4;

  if (matchedUser != 0) {
    delay(1500);
    processTransaction(matchedUser);
  } else {
    Serial.println(F("Error: Card Mismatch"));
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Error: "));
    lcd.setCursor(0, 1);
    lcd.print(F("Card Mismatch"));
    delay(3000);
  }

  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  deductMode = false;
  deductAmountEntered = false;
  amountBufferCents = 0;
  digitIndex = 0;

  showIdleScreen();
}