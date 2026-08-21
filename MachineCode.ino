#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

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

void setup() {
  // Setting up/Initializing all components:
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
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uID = getUIDString(rfid);
  String currentuID = uID;
  //Serial.print("Scanned Card: ");
  clearLine(1);
  clearLine(2);
  clearLine(3);
  //lcd.setCursor(0, 1);
  //lcd.print("Scanned: ");

  Serial.println(uID);
  lcd.setCursor(0, 2);
  lcd.print(uID);

  if (uID == target) {
    processTransaction();
    delay(2000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Card Mismatch");

    Serial.println("Error: Card Mismatch");
    delay(3000);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  Serial.println();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Card: ");
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
  } else {
    Serial.println("Error: Insufficient Funds");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: ");
    lcd.setCursor(0, 1);
    lcd.print("Insufficient Funds");
  }
}