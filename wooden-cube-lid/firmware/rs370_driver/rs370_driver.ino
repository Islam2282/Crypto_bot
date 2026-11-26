/*
  Два мотора RS-370 + один драйвер L298N + Arduino Nano: пониженные обороты.

  У L298N два канала: A (OUT1/OUT2) и B (OUT3/OUT4) — по одному на мотор.
  Скорость задаёт ШИМ на ENA / ENB (0..255), направление — IN1..IN4.
  Держите кнопку «вперёд» — оба мотора крутятся вперёд, «назад» — назад,
  отпустили — стоп. Старт плавный, с коротким толчком.

  Подключение (L298N):
    батарея +  → предохранитель → выключатель → L298N «12V» и Arduino VIN
    батарея −  → L298N «GND» и Arduino GND (общий минус)
    L298N «5V» — не подключать (перемычка 5V_EN на плате остаётся)
    мотор 1 → OUT1, OUT2        мотор 2 → OUT3, OUT4
    снимите перемычки с ENA и ENB!
    ENA → D5    IN1 → D7    IN2 → D8      (мотор 1)
    ENB → D6    IN3 → D9    IN4 → D10     (мотор 2)
    кнопка «вперёд» → D2 и GND,  кнопка «назад» → D3 и GND
    (радиореле: COM каналов → GND, NO канала A → D2, NO канала B → D3)
    по желанию: потенциометр 10 кОм — крайние выводы на 5V и GND, средний на A0

  ВАЖНО: ШИМ снижает и обороты, и силу мотора. Тяжёлое голый RS-370 не поднимет.
*/

// ---------- настройки ----------
#define USE_POT 0                  // 1 = скорость обоих крутилкой на A0, 0 = константы SPEED_1 / SPEED_2
const uint8_t SPEED_1  = 90;       // 0..255, мотор 1 (примерно 35 %)
const uint8_t SPEED_2  = 90;       // 0..255, мотор 2 — подстройте, если крутится быстрее/медленнее первого
const bool    INVERT_2 = false;    // true — мотор 2 крутится в обратную сторону (вместо перестановки проводов)
const uint8_t MIN_PWM  = 40;       // ниже этого мотор обычно просто гудит
const uint8_t KICK_PWM = 200;      // толчок при старте
const unsigned long KICK_MS = 80;
const unsigned long RAMP_MS = 400; // плавный разгон
const uint8_t MAX_PWM  = 255;      // для моторов на 6 В при батарее 11–12 В поставьте 128

// ---------- пины ----------
struct Motor { uint8_t en, inA, inB, speed; bool invert; };
Motor motors[2] = {
  { 5, 7, 8,  SPEED_1, false },
  { 6, 9, 10, SPEED_2, INVERT_2 },
};
const uint8_t PIN_FWD = 2;
const uint8_t PIN_BACK = 3;
const uint8_t PIN_POT = A0;

int dir = 0;                         // +1 вперёд, -1 назад, 0 стоп
unsigned long startMs = 0;

uint8_t clampSpeed(int s) {
  if (s <= 0) return 0;
  if (s > MAX_PWM) s = MAX_PWM;
  if (s < MIN_PWM) s = MIN_PWM;
  return s;
}

void setMotorDir(const Motor &m, int d) {
  if (m.invert) d = -d;
  analogWrite(m.en, 0);
  digitalWrite(m.inA, d > 0 ? HIGH : LOW);
  digitalWrite(m.inB, d < 0 ? HIGH : LOW);
}

void setDirection(int d) {
  dir = d;
  for (const Motor &m : motors) setMotorDir(m, d);
  startMs = millis();
}

void setup() {
  for (const Motor &m : motors) { pinMode(m.en, OUTPUT); pinMode(m.inA, OUTPUT); pinMode(m.inB, OUTPUT); }
  pinMode(PIN_FWD, INPUT_PULLUP);
  pinMode(PIN_BACK, INPUT_PULLUP);
  setDirection(0);
  Serial.begin(115200);
  Serial.println(F("2 x RS-370: держите кнопку D2 (вперёд) или D3 (назад)"));
}

void loop() {
  bool fwd  = digitalRead(PIN_FWD) == LOW;
  bool back = digitalRead(PIN_BACK) == LOW;
  int want = (fwd && !back) ? 1 : (back && !fwd) ? -1 : 0;

  if (want != dir) {
    if (dir != 0 && want != 0) { setDirection(0); delay(150); }  // пауза перед разворотом
    setDirection(want);
    Serial.println(want > 0 ? F("Вперёд") : want < 0 ? F("Назад") : F("Стоп"));
  }
  if (dir == 0) return;

  unsigned long t = millis() - startMs;
  for (const Motor &m : motors) {
#if USE_POT
    uint8_t s = clampSpeed(map(analogRead(PIN_POT), 0, 1023, 0, 255));   // крутилка задаёт скорость обоим
#else
    uint8_t s = clampSpeed(m.speed);
#endif
    uint8_t pwm;
    if (s == 0)                          pwm = 0;
    else if (t < KICK_MS)                pwm = KICK_PWM < MAX_PWM ? KICK_PWM : MAX_PWM;
    else if (t < KICK_MS + RAMP_MS)      pwm = MIN_PWM + (long)(s - MIN_PWM) * (t - KICK_MS) / RAMP_MS;
    else                                 pwm = s;
    analogWrite(m.en, pwm);
  }
}
