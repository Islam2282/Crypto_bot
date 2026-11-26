/*
  Два мотора RS-370 (12 В) + L298N + Arduino Nano. Без кнопок:
  включили питание — оба мотора плавно разгоняются и крутятся на пониженных оборотах.

  Подключение (L298N):
    батарея +  → предохранитель → выключатель → L298N «12V» и Arduino VIN
    батарея −  → L298N «GND» и Arduino GND
    L298N «5V» — не подключать
    мотор 1 → OUT1, OUT2        мотор 2 → OUT3, OUT4
    снимите перемычки с ENA и ENB!
    ENA → D5    IN1 → D7    IN2 → D8
    ENB → D6    IN3 → D9    IN4 → D10
*/

const uint8_t SPEED_1 = 130;   // 0..255: обороты мотора 1 (130 ≈ половина)
const uint8_t SPEED_2 = 130;   // 0..255: обороты мотора 2
const bool    REVERSE_1 = false;  // true — мотор 1 крутится в другую сторону
const bool    REVERSE_2 = false;  // true — мотор 2 крутится в другую сторону

const uint8_t ENA = 5, IN1 = 7, IN2 = 8;
const uint8_t ENB = 6, IN3 = 9, IN4 = 10;

void setup() {
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // направление
  digitalWrite(IN1, REVERSE_1 ? LOW : HIGH);  digitalWrite(IN2, REVERSE_1 ? HIGH : LOW);
  digitalWrite(IN3, REVERSE_2 ? LOW : HIGH);  digitalWrite(IN4, REVERSE_2 ? HIGH : LOW);

  // короткий толчок, чтобы моторы стронулись на малых оборотах
  analogWrite(ENA, 220); analogWrite(ENB, 220);
  delay(100);

  // дальше заданные обороты
  analogWrite(ENA, SPEED_1);
  analogWrite(ENB, SPEED_2);
}

void loop() {
  // ничего не делаем: моторы крутятся, пока есть питание
}
