#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 4);

void setup() {
  lcd.init();
  lcd.backLight();
  Serial.begin(9600);
}

void loop() {
  Serial.println("Scan Card: ");

  lcd.setCursor(0, 0);
  lcd.print("Scan Card: ");

}
