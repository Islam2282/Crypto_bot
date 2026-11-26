/*
  Проверка сервоприводов.
  По очереди плавно качает левый (D9) и правый (D10) сервопривод
  примерно на ±45° от середины и пишет в монитор порта (115200), кто сейчас двигается.

  Перед проверкой снимите тяги с качалок, чтобы ничего не упёрлось.

  Как читать результат:
   - левый качается, правый тоже → проводка в порядке, дело в основном скетче или механике;
   - левый не качается → поменяйте оранжевые провода местами (левый на D10, правый на D9):
       * если теперь качается левый, а правый стоит → плохой контакт на D9 или сам пин D9.
         Переставьте оранжевый провод левого на D6 и в servo_lid.ino поменяйте
         PIN_SERVO_L = 9 на PIN_SERVO_L = 6;
       * если левый всё равно стоит → дело в самом левом сервоприводе или его проводах
         (красный/коричневый не зажаты в LM2596, отошёл разъём, сгорел сервопривод).
*/

#include <Servo.h>

const uint8_t PIN_LEFT  = 9;   // поменяйте, если перенесли провод на другой пин
const uint8_t PIN_RIGHT = 10;

Servo servoLeft, servoRight;

void sweep(Servo &s) {
  for (int us = 1500; us <= 2000; us += 10) { s.writeMicroseconds(us); delay(15); }
  for (int us = 2000; us >= 1000; us -= 10) { s.writeMicroseconds(us); delay(15); }
  for (int us = 1000; us <= 1500; us += 10) { s.writeMicroseconds(us); delay(15); }
}

void setup() {
  Serial.begin(115200);
  servoLeft.attach(PIN_LEFT, 500, 2500);
  servoRight.attach(PIN_RIGHT, 500, 2500);
  servoLeft.writeMicroseconds(1500);   // оба в середину
  servoRight.writeMicroseconds(1500);
  Serial.println(F("Проверка сервоприводов"));
  delay(1000);
}

void loop() {
  Serial.println(F("Двигаю ЛЕВЫЙ (D9)"));
  sweep(servoLeft);
  delay(1000);
  Serial.println(F("Двигаю ПРАВЫЙ (D10)"));
  sweep(servoRight);
  delay(1000);
}
