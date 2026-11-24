/*
  ================================================================
   Деревянный куб 50×50×50 см с автоматической крышкой на пульте
  ================================================================
   Плата:      Arduino Nano (ATmega328P) / Uno
   Привод:     линейный актуатор 12 В, ход 150 мм, 200–300 Н
               (со встроенными концевиками — они есть почти у всех)
   Силовая:    2-канальный модуль реле 5 В (10 А), собран как H-мост
   Пульт:      радио 433 МГц (брелок EV1527/PT2262 + приёмник RXB6)
               или любой ИК-пульт (приёмник VS1838B) — см. REMOTE_TYPE
   Датчики:    концевик "крышка закрыта" (микрик или геркон),
               по желанию: концевик "открыта", датчик тока ACS712

   Логика одной кнопки пульта (как у гаражных ворот):
     закрыта  -> открывается
     открыта  -> закрывается
     едет     -> стоп
     стоп     -> едет в обратную сторону

   Привязка пульта: удерживайте кнопку на корпусе 3 секунды
   (светодиод часто мигает) и нажмите кнопку на пульте.
   Код сохраняется в EEPROM и не теряется при отключении питания.

   Библиотеки (Arduino IDE -> Менеджер библиотек):
     REMOTE_TYPE 1: "rc-switch" (автор sui77)
     REMOTE_TYPE 2: "IRremote" (Armin Joachimsmeyer), версия 4.x
  ================================================================
*/

#include <EEPROM.h>

// ======================= НАСТРОЙКИ =======================

// 1 = радиопульт 433 МГц, 2 = ИК-пульт
#define REMOTE_TYPE 1

// 1 = датчик установлен, 0 = нет
#define USE_LIMIT_CLOSED   1   // концевик "крышка закрыта" (рекомендуется)
#define USE_LIMIT_OPEN     0   // концевик "крышка открыта" (необязательно)
#define USE_CURRENT_SENSOR 0   // ACS712 для защиты от защемления

// Большинство китайских модулей реле включаются НИЗКИМ уровнем (LOW).
// Если у вас реле щёлкает наоборот, поставьте 0.
#define RELAY_ACTIVE_LOW 1

// Время полного хода актуатора: ход / скорость.
// 150 мм / 20 мм/с = 7,5 с. Измерьте секундомером и впишите своё.
const unsigned long TRAVEL_TIME_MS      = 7500;
// Сколько держать реле включённым: ход + 30 % запаса.
// В конце хода актуатор сам отключится своим внутренним концевиком.
const unsigned long MOVE_TIMEOUT_MS     = TRAVEL_TIME_MS * 13 / 10 + 500;
// После срабатывания концевика "закрыто" ещё чуть дотянуть крышку
const unsigned long CLOSE_OVERRUN_MS    = 250;
// Пауза между остановкой и пуском в обратную сторону (бережёт реле и мотор)
const unsigned long DIR_CHANGE_PAUSE_MS = 300;

const unsigned long LEARN_HOLD_MS       = 3000;  // удержание кнопки для привязки
const unsigned long LEARN_TIMEOUT_MS    = 15000; // сколько ждать код пульта
const unsigned long REMOTE_RELEASE_MS   = 400;   // пауза = новое нажатие пульта
const unsigned long BUTTON_DEBOUNCE_MS  = 30;

// Датчик тока ACS712 (только при USE_CURRENT_SENSOR 1)
const float ACS_MV_PER_A                = 185.0; // 5A: 185, 20A: 100, 30A: 66 мВ/А
const float OBSTACLE_CURRENT_A          = 2.5;   // порог "крышка во что-то упёрлась"
const unsigned long INRUSH_IGNORE_MS    = 400;   // пусковой ток не считаем

// ======================= ПИНЫ =======================
const uint8_t PIN_REMOTE       = 2;   // DATA приёмника 433 МГц / OUT ИК-приёмника
const uint8_t PIN_BUTTON       = 4;   // кнопка на корпусе -> GND
const uint8_t PIN_LIMIT_CLOSED = 5;   // концевик "закрыто" (NO) -> GND
const uint8_t PIN_LIMIT_OPEN   = 6;   // концевик "открыто" (NO) -> GND
const uint8_t PIN_RELAY_OPEN   = 7;   // IN1 модуля реле: шток выдвигается
const uint8_t PIN_RELAY_CLOSE  = 8;   // IN2 модуля реле: шток втягивается
const uint8_t PIN_LED          = 13;  // встроенный светодиод (+ можно вывести наружу)
const uint8_t PIN_CURRENT      = A0;  // OUT датчика ACS712

// ======================= ПУЛЬТ =======================
#if REMOTE_TYPE == 1
  #include <RCSwitch.h>
  RCSwitch rf;
#elif REMOTE_TYPE == 2
  #include <IRremote.hpp>
#else
  #error "REMOTE_TYPE должен быть 1 (433 МГц) или 2 (ИК)"
#endif

// ======================= СОСТОЯНИЕ =======================
enum LidState : uint8_t { CLOSED, OPENING, OPEN, CLOSING, STOPPED };
enum MotorDir : uint8_t { MOTOR_STOP, MOTOR_EXTEND, MOTOR_RETRACT };

const char* const STATE_NAMES[] = { "ЗАКРЫТО", "ОТКРЫВАЕТСЯ", "ОТКРЫТО", "ЗАКРЫВАЕТСЯ", "СТОП" };

LidState state        = STOPPED;
LidState lastMove     = OPENING;   // при неизвестном положении первое нажатие закроет крышку
MotorDir motor        = MOTOR_STOP;
unsigned long moveStartMs   = 0;
unsigned long motorStopMs   = 0;
unsigned long closedSeenMs  = 0;   // когда сработал концевик "закрыто" (0 = ещё нет)
bool fault            = false;     // крышка не дошла до концевика за отведённое время

bool learning         = false;
unsigned long learnStartMs = 0;

// EEPROM: сохранённый код пульта
const int      EE_ADDR  = 0;
const uint16_t EE_MAGIC = 0xB0C5;
struct SavedRemote { uint16_t magic; uint32_t code; };
uint32_t remoteCode = 0;           // 0 = пульт не привязан

#if USE_CURRENT_SENSOR
float currentZeroMv = 2500.0;
#endif

// ======================= РЕЛЕ / МОТОР =======================
void relayWrite(uint8_t pin, bool on) {
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(pin, level ? HIGH : LOW);
}

// Схема H-моста на двух реле:
//   оба выключены -> оба провода мотора на "минусе" (тормоз)
//   реле 1        -> выдвижение штока (открыть)
//   реле 2        -> втягивание штока (закрыть)
// Оба реле одновременно никогда не включаются.
void setMotor(MotorDir dir) {
  if (dir == motor) return;

  relayWrite(PIN_RELAY_OPEN, false);
  relayWrite(PIN_RELAY_CLOSE, false);

  if (dir == MOTOR_STOP) {
    motor = MOTOR_STOP;
    motorStopMs = millis();
    return;
  }

  // Мотор только что крутился: даём ему остановиться перед пуском
  if (motor != MOTOR_STOP) motorStopMs = millis();
  unsigned long sinceStop = millis() - motorStopMs;
  if (sinceStop < DIR_CHANGE_PAUSE_MS) delay(DIR_CHANGE_PAUSE_MS - sinceStop);

  relayWrite(dir == MOTOR_EXTEND ? PIN_RELAY_OPEN : PIN_RELAY_CLOSE, true);
  motor = dir;
}

// ======================= ДАТЧИКИ =======================
bool limitClosed() {
#if USE_LIMIT_CLOSED
  return digitalRead(PIN_LIMIT_CLOSED) == LOW;
#else
  return false;
#endif
}

bool limitOpen() {
#if USE_LIMIT_OPEN
  return digitalRead(PIN_LIMIT_OPEN) == LOW;
#else
  return false;
#endif
}

#if USE_CURRENT_SENSOR
float readMilliVolts(uint8_t samples) {
  long sum = 0;
  for (uint8_t i = 0; i < samples; i++) sum += analogRead(PIN_CURRENT);
  return (sum / (float)samples) * 5000.0 / 1023.0;
}

float readCurrentA() {
  float a = (readMilliVolts(8) - currentZeroMv) / ACS_MV_PER_A;
  return a < 0 ? -a : a;
}
#endif

// ======================= ПЕРЕХОДЫ =======================
void setState(LidState s) {
  if (s == state) return;
  state = s;
  Serial.print(F("Крышка: "));
  Serial.println(STATE_NAMES[s]);
}

void startOpening() {
  fault = false;
  if (limitOpen()) { setState(OPEN); return; }
  setMotor(MOTOR_EXTEND);
  moveStartMs = millis();
  lastMove = OPENING;
  setState(OPENING);
}

void startClosing() {
  fault = false;
  if (limitClosed()) { setState(CLOSED); return; }
  setMotor(MOTOR_RETRACT);
  moveStartMs = millis();
  closedSeenMs = 0;
  lastMove = CLOSING;
  setState(CLOSING);
}

void stopMotion() {
  setMotor(MOTOR_STOP);
  setState(STOPPED);
}

// Одна команда с пульта или кнопки
void onCommand() {
  switch (state) {
    case CLOSED:  startOpening(); break;
    case OPEN:    startClosing(); break;
    case OPENING:
    case CLOSING: stopMotion();   break;
    case STOPPED:
      if (lastMove == OPENING) startClosing();
      else                     startOpening();
      break;
  }
}

void updateMotion() {
  if (state != OPENING && state != CLOSING) return;
  unsigned long now = millis();
  unsigned long elapsed = now - moveStartMs;

#if USE_CURRENT_SENSOR
  // Защита от защемления: ток резко вырос -> крышка во что-то упёрлась
  if (elapsed > INRUSH_IGNORE_MS && closedSeenMs == 0) {
    float amps = readCurrentA();
    if (amps > OBSTACLE_CURRENT_A) {
      Serial.print(F("Препятствие! Ток, А: "));
      Serial.println(amps);
      if (state == CLOSING) startOpening();  // закрывалась -> открыть обратно
      else                  stopMotion();    // открывалась -> просто стоп
      return;
    }
  }
#endif

  if (state == OPENING) {
    if (limitOpen()) {
      setMotor(MOTOR_STOP);
      setState(OPEN);
    } else if (elapsed >= MOVE_TIMEOUT_MS) {
      setMotor(MOTOR_STOP);
      // Без концевика "открыто" считаем, что актуатор дошёл до упора сам
      if (USE_LIMIT_OPEN) { fault = true; setState(STOPPED); }
      else                setState(OPEN);
    }
    return;
  }

  // state == CLOSING
  if (closedSeenMs == 0 && limitClosed()) closedSeenMs = now ? now : 1;

  if (closedSeenMs != 0 && now - closedSeenMs >= CLOSE_OVERRUN_MS) {
    setMotor(MOTOR_STOP);
    setState(CLOSED);
  } else if (elapsed >= MOVE_TIMEOUT_MS) {
    setMotor(MOTOR_STOP);
    if (USE_LIMIT_CLOSED && closedSeenMs == 0) {
      fault = true;              // крышка не закрылась: что-то мешает
      Serial.println(F("Ошибка: концевик 'закрыто' не сработал"));
      setState(STOPPED);
    } else {
      setState(CLOSED);
    }
  }
}

// ======================= ПУЛЬТ =======================
// Сырой код из приёмника, 0 = ничего не пришло
uint32_t readRemoteRaw() {
#if REMOTE_TYPE == 1
  if (rf.available()) {
    uint32_t v = rf.getReceivedValue();
    rf.resetAvailable();
    return v;
  }
#else
  static uint32_t lastIr = 0;
  if (IrReceiver.decode()) {
    uint32_t v;
    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      v = lastIr;                // кнопку держат: тот же код
    } else {
      v = IrReceiver.decodedIRData.decodedRawData;
      lastIr = v;
    }
    IrReceiver.resume();
    return v;
  }
#endif
  return 0;
}

// Код возвращается только в момент нового нажатия:
// пульт шлёт код много раз, пока кнопку держат
uint32_t pollRemote() {
  static uint32_t lastCode = 0;
  static unsigned long lastRxMs = 0;

  uint32_t c = readRemoteRaw();
  if (c == 0) return 0;

  unsigned long now = millis();
  bool newPress = (c != lastCode) || (now - lastRxMs > REMOTE_RELEASE_MS);
  lastCode = c;
  lastRxMs = now;
  return newPress ? c : 0;
}

void loadRemoteCode() {
  SavedRemote s;
  EEPROM.get(EE_ADDR, s);
  remoteCode = (s.magic == EE_MAGIC) ? s.code : 0;
}

void saveRemoteCode(uint32_t code) {
  SavedRemote s = { EE_MAGIC, code };
  EEPROM.put(EE_ADDR, s);
  remoteCode = code;
}

void blinkLed(uint8_t times, unsigned int ms) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(PIN_LED, HIGH); delay(ms);
    digitalWrite(PIN_LED, LOW);  delay(ms);
  }
}

void startLearning() {
  learning = true;
  learnStartMs = millis();
  Serial.println(F("Привязка: нажмите кнопку на пульте"));
}

void handleRemote() {
  uint32_t code = pollRemote();

  if (learning) {
    if (code != 0) {
      saveRemoteCode(code);
      learning = false;
      Serial.print(F("Пульт привязан, код: "));
      Serial.println(code);
      blinkLed(3, 150);
    } else if (millis() - learnStartMs > LEARN_TIMEOUT_MS) {
      learning = false;
      Serial.println(F("Привязка отменена (нет сигнала)"));
    }
    return;
  }

  if (code == 0) return;
  if (remoteCode != 0 && code == remoteCode) {
    onCommand();
  } else {
    Serial.print(F("Чужой код: "));
    Serial.println(code);
  }
}

// ======================= КНОПКА НА КОРПУСЕ =======================
// Короткое нажатие = команда, удержание 3 с = привязка пульта.
// Если крышка едет, нажатие останавливает её сразу.
void handleButton() {
  static bool stable = HIGH, lastRead = HIGH;
  static unsigned long changedMs = 0, pressedMs = 0;
  static bool consumed = false;

  bool r = digitalRead(PIN_BUTTON);
  unsigned long now = millis();
  if (r != lastRead) { lastRead = r; changedMs = now; }
  if (now - changedMs < BUTTON_DEBOUNCE_MS) return;

  if (r != stable) {
    stable = r;
    if (stable == LOW) {                         // нажали
      pressedMs = now;
      consumed = false;
      if (state == OPENING || state == CLOSING) { stopMotion(); consumed = true; }
      else if (learning) { learning = false; consumed = true;
                           Serial.println(F("Привязка отменена")); }
    } else if (!consumed) {                      // отпустили
      onCommand();
    }
  }

  if (stable == LOW && !consumed && now - pressedMs >= LEARN_HOLD_MS) {
    consumed = true;
    startLearning();
  }
}

// ======================= СВЕТОДИОД =======================
void updateLed() {
  unsigned long t = millis();
  bool on;
  if (learning)                        on = (t / 80) % 2;          // очень часто
  else if (fault)                      on = (t % 1000) < 100 || ((t % 1000) > 200 && (t % 1000) < 300); // двойная вспышка
  else switch (state) {
    case CLOSED:  on = false; break;
    case OPEN:    on = true;  break;
    case STOPPED: on = (t / 1000) % 2; break;                       // медленно
    default:      on = (t / 200) % 2;  break;                       // едет
  }
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

// ======================= SETUP / LOOP =======================
void setup() {
  // Сначала "выключено", потом OUTPUT — чтобы реле не щёлкнули при старте
  relayWrite(PIN_RELAY_OPEN, false);
  relayWrite(PIN_RELAY_CLOSE, false);
  pinMode(PIN_RELAY_OPEN, OUTPUT);
  pinMode(PIN_RELAY_CLOSE, OUTPUT);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LIMIT_CLOSED, INPUT_PULLUP);
  pinMode(PIN_LIMIT_OPEN, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println(F("\nКуб с крышкой: старт"));

#if REMOTE_TYPE == 1
  rf.enableReceive(digitalPinToInterrupt(PIN_REMOTE));
#else
  IrReceiver.begin(PIN_REMOTE, DISABLE_LED_FEEDBACK);
#endif

#if USE_CURRENT_SENSOR
  delay(100);
  currentZeroMv = readMilliVolts(64);   // ноль датчика при выключенном моторе
#endif

  loadRemoteCode();
  if (remoteCode == 0) Serial.println(F("Пульт не привязан: удерживайте кнопку 3 с"));
  else { Serial.print(F("Код пульта: ")); Serial.println(remoteCode); }

  // Начальное положение
  if (limitClosed())     { state = CLOSED; lastMove = CLOSING; }
  else if (limitOpen())  { state = OPEN;   lastMove = OPENING; }
  else                   { state = STOPPED; lastMove = OPENING; } // первое нажатие закроет
  Serial.print(F("Крышка: "));
  Serial.println(STATE_NAMES[state]);
}

void loop() {
  handleButton();
  handleRemote();
  updateMotion();
  updateLed();
}
