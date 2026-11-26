/*
  Мотор RS-370 + драйвер L298N + Arduino Nano: пониженные обороты.

  Скорость задаёт ШИМ на ENA драйвера (0..255), направление — IN1/IN2.
  Кнопки (или контакты радиореле): держите «вперёд» — мотор крутится вперёд,
  держите «назад» — назад, отпустили — стоп. Старт плавный, с коротким толчком,
  чтобы мотор сдвинулся и на малой скорости.

  Подключение (L298N):
    батарея +  → предохранитель → выключатель → L298N «12V»
    батарея −  → L298N «GND» и Arduino GND (общий минус)
    L298N «5V» — не подключать (перемычка 5V_EN на плате остаётся)
    OUT1, OUT2 → два провода мотора
    ENA        → Arduino D5  (сначала снимите перемычку с ENA!)
    IN1        → Arduino D7
    IN2        → Arduino D8
    кнопка «вперёд» → D2 и GND,  кнопка «назад» → D3 и GND
    (радиореле: COM канала → GND, NO канала A → D2, NO канала B → D3)
    по желанию: потенциометр 10 кОм — крайние выводы на 5V и GND, средний на A0

  ВАЖНО: ШИМ снижает и обороты, и силу мотора. Тяжёлое (крышку) голый RS-370
  так не поднимет — для этого нужен редуктор.
*/

// ---------- настройки ----------
#define USE_POT 0                  // 1 = скорость крутилкой на A0, 0 = константа SPEED
const uint8_t SPEED    = 90;       // 0..255, примерно 35 % оборотов
const uint8_t MIN_PWM  = 40;       // ниже этого мотор обычно просто гудит
const uint8_t KICK_PWM = 200;      // толчок при старте
const unsigned long KICK_MS = 80;
const unsigned long RAMP_MS = 400; // плавный разгон до SPEED

// если мотор рассчитан на 6 В, а батарея 11–12 В, не давайте больше половины
const uint8_t MAX_PWM = 255;       // для мотора на 6 В поставьте 128

// ---------- пины ----------
const uint8_t PIN_ENA = 5;   // ШИМ
const uint8_t PIN_IN1 = 7;
const uint8_t PIN_IN2 = 8;
const uint8_t PIN_FWD = 2;
const uint8_t PIN_BACK = 3;
const uint8_t PIN_POT = A0;

int dir = 0;                         // +1 вперёд, -1 назад, 0 стоп
unsigned long startMs = 0;

uint8_t targetSpeed() {
#if USE_POT
  uint8_t s = map(analogRead(PIN_POT), 0, 1023, 0, 255);
#else
  uint8_t s = SPEED;
#endif
  if (s > MAX_PWM) s = MAX_PWM;
  if (s > 0 && s < MIN_PWM) s = MIN_PWM;
  return s;
}

void setDirection(int d) {
  dir = d;
  analogWrite(PIN_ENA, 0);
  if (d > 0)      { digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);  }
  else if (d < 0) { digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH); }
  else            { digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, LOW);  }
  startMs = millis();
}

void setup() {
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_FWD, INPUT_PULLUP);
  pinMode(PIN_BACK, INPUT_PULLUP);
  setDirection(0);
  Serial.begin(115200);
  Serial.println(F("RS-370: держите кнопку D2 (вперёд) или D3 (назад)"));
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
  uint8_t s = targetSpeed();
  uint8_t pwm;
  if (s == 0)                     pwm = 0;                                        // крутилка на нуле
  else if (t < KICK_MS)           pwm = KICK_PWM < MAX_PWM ? KICK_PWM : MAX_PWM;  // толчок
  else if (t < KICK_MS + RAMP_MS) pwm = MIN_PWM + (long)(s - MIN_PWM) * (t - KICK_MS) / RAMP_MS;
  else                            pwm = s;
  analogWrite(PIN_ENA, pwm);
}
