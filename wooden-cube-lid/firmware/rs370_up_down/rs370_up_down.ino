/*
  Подъём / спуск: 2 мотора RS-370 (12 В) + L298N + Arduino Nano + радиореле 433 МГц.

  Брелок: держите A — подъём, держите B — спуск, отпустили — стоп.
  (Радиореле в режиме «без фиксации»: реле включено, пока держите кнопку.)

  Подключение:
    батарея +  → предохранитель → выключатель → радиореле +12V, L298N «12V», Arduino VIN
    батарея −  → радиореле GND, L298N GND, Arduino GND
    радиореле канал A: COM → Arduino GND, NO → D2   (подъём)
    радиореле канал B: COM → Arduino GND, NO → D3   (спуск)
    мотор 1 → L298N OUT1, OUT2       мотор 2 → L298N OUT3, OUT4
    снимите перемычки с ENA и ENB!
    ENA → D5   IN1 → D7   IN2 → D8
    ENB → D6   IN3 → D9   IN4 → D10
*/

const uint8_t SPEED_1 = 130;      // 0..255: обороты мотора 1
const uint8_t SPEED_2 = 130;      // 0..255: обороты мотора 2
const bool REVERSE_1 = false;     // true — мотор 1 крутится в другую сторону
const bool REVERSE_2 = false;     // true — мотор 2 крутится в другую сторону

const uint8_t PIN_UP = 2, PIN_DOWN = 3;
const uint8_t ENA = 5, IN1 = 7, IN2 = 8;
const uint8_t ENB = 6, IN3 = 9, IN4 = 10;

int state = 0;   // +1 подъём, -1 спуск, 0 стоп

void motors(int d) {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  int d1 = REVERSE_1 ? -d : d;
  int d2 = REVERSE_2 ? -d : d;
  digitalWrite(IN1, d1 > 0 ? HIGH : LOW);
  digitalWrite(IN2, d1 < 0 ? HIGH : LOW);
  digitalWrite(IN3, d2 > 0 ? HIGH : LOW);
  digitalWrite(IN4, d2 < 0 ? HIGH : LOW);
  if (d != 0) {
    analogWrite(ENA, 220);          // короткий толчок, чтобы моторы стронулись
    analogWrite(ENB, 220);
    delay(100);
    analogWrite(ENA, SPEED_1);
    analogWrite(ENB, SPEED_2);
  }
}

void setup() {
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  motors(0);
}

void loop() {
  bool up   = digitalRead(PIN_UP) == LOW;
  bool down = digitalRead(PIN_DOWN) == LOW;
  int want = (up && !down) ? 1 : (down && !up) ? -1 : 0;

  if (want != state) {
    if (state != 0 && want != 0) { motors(0); delay(150); }  // пауза перед разворотом
    motors(want);
    state = want;
  }
  delay(10);
}
