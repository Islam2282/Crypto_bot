/*
  ================================================================
   Куб 50×50×50 см, крышка из двух половин на сервоприводах
  ================================================================
   Плата:    Arduino Nano
   Приводы:  2 × MG996R, по одному на половину крышки.
             Качалка 30 мм (между осью и отверстием) → тяга 73 мм → уголок под крышкой.
   Пульт:    радиореле 433 МГц на 4 канала, его реле работают как кнопки:
               канал A: COM → GND, NO → D2   (открыть)
               канал B: COM → GND, NO → D3   (закрыть)
               канал C: COM → GND, NO → D4   (стоп, по желанию)
   Питание:  8 × AA (12 В) → выключатель → радиореле 12 В и Arduino VIN;
             сервоприводы через понижающий LM2596, выставленный на 6,0 В.
             Минусы всего соединить вместе.

   Нажали A — обе половины плавно открываются за MOVE_MS.
   Нажали B — закрываются. Во время движения можно нажать другую кнопку,
   крышка развернётся с того места, где была. C — остановиться.

   Настройка (один раз, до того как надеть тяги):
     1. Включите. Сервоприводы встанут в положение «закрыто».
     2. Наденьте качалки так, чтобы они смотрели вниз и чуть к центру ящика,
        в сторону от уголка крышки. Это положение «закрыто».
     3. Нажмите A. Качалка должна повернуться примерно на пол-оборота через верх
        и смотреть вверх, на уголок крышки. Если крутится через стенку —
        поменяйте местами CLOSED_US и OPEN_US у этого сервопривода.
     4. Наденьте тяги. Если половина не доходит или упирается,
        подправьте OPEN_US / CLOSED_US на 30–50 мкс.
  ================================================================
*/

#include <Servo.h>
#include <EEPROM.h>

// ---------- пины ----------
const uint8_t PIN_OPEN    = 2;   // канал A радиореле
const uint8_t PIN_CLOSE   = 3;   // канал B
const uint8_t PIN_STOP    = 4;   // канал C (можно не подключать)
const uint8_t PIN_SERVO_L = 9;
const uint8_t PIN_SERVO_R = 10;

// ---------- положения сервоприводов, мкс ----------
// Левый и правый стоят зеркально, поэтому у правого значения наоборот.
const int L_CLOSED_US = 2450, L_OPEN_US = 550;
const int R_CLOSED_US = 550,  R_OPEN_US = 2450;

// ---------- движение ----------
const unsigned long MOVE_MS = 3000;  // полное открытие или закрытие
const unsigned long HOLD_MS = 700;   // после остановки держим, потом отпускаем сервоприводы
const unsigned long DEBOUNCE_MS = 30;

Servo servoL, servoR;
float pos = 0;          // 0 = закрыто, 1 = открыто
float fromPos = 0, target = 0;
unsigned long moveStartMs = 0, moveDurMs = 0, stoppedMs = 0;
bool moving = false, attached = false;

int lerpUs(int a, int b, float p) { return a + (int)((b - a) * p); }
float easeInOut(float t) { return t < 0.5 ? 2 * t * t : 1 - (-2 * t + 2) * (-2 * t + 2) / 2; }

void writeServos() {
  servoL.writeMicroseconds(lerpUs(L_CLOSED_US, L_OPEN_US, pos));
  servoR.writeMicroseconds(lerpUs(R_CLOSED_US, R_OPEN_US, pos));
}

void attachServos() {
  if (attached) return;
  servoL.attach(PIN_SERVO_L, 500, 2500);
  servoR.attach(PIN_SERVO_R, 500, 2500);
  writeServos();
  attached = true;
}

void detachServos() {
  if (!attached) return;
  servoL.detach();
  servoR.detach();
  attached = false;
}

void startMove(float to) {
  if (moving && to == target) return;
  if (!moving && pos == to) return;
  fromPos = pos;
  target = to;
  moveDurMs = (unsigned long)(MOVE_MS * (to > pos ? to - pos : pos - to));
  if (moveDurMs < 50) moveDurMs = 50;
  moveStartMs = millis();
  moving = true;
  attachServos();
  Serial.println(to > 0.5 ? F("Открываю") : F("Закрываю"));
}

void stopMove() {
  if (!moving) return;
  moving = false;
  stoppedMs = millis();
  Serial.println(F("Стоп"));
}

// нажатие = контакт реле замкнул вход на GND; срабатываем по фронту
struct Button {
  uint8_t pin; bool stable; bool last; unsigned long changedMs;
  bool pressedEdge() {
    bool r = digitalRead(pin) == LOW;
    unsigned long now = millis();
    if (r != last) { last = r; changedMs = now; }
    if (now - changedMs >= DEBOUNCE_MS && r != stable) {
      stable = r;
      return stable;
    }
    return false;
  }
};
Button btnOpen  = { PIN_OPEN,  false, false, 0 };
Button btnClose = { PIN_CLOSE, false, false, 0 };
Button btnStop  = { PIN_STOP,  false, false, 0 };

void setup() {
  pinMode(PIN_OPEN, INPUT_PULLUP);
  pinMode(PIN_CLOSE, INPUT_PULLUP);
  pinMode(PIN_STOP, INPUT_PULLUP);
  Serial.begin(115200);

  // при включении встаём в последнее сохранённое положение, без рывка через весь ход
  pos = (EEPROM.read(0) == 1) ? 1.0 : 0.0;
  target = pos;
  attachServos();
  stoppedMs = millis();
  Serial.println(pos > 0.5 ? F("Старт: открыто") : F("Старт: закрыто"));
}

void loop() {
  unsigned long now = millis();

  bool a = btnOpen.pressedEdge();
  bool b = btnClose.pressedEdge();
  bool c = btnStop.pressedEdge();
  if (c) stopMove();
  else if (a && !b) startMove(1.0);
  else if (b && !a) startMove(0.0);

  if (moving) {
    float t = (float)(now - moveStartMs) / moveDurMs;
    if (t >= 1) {
      t = 1;
      moving = false;
      stoppedMs = now;
      EEPROM.update(0, target > 0.5 ? 1 : 0);
      Serial.println(target > 0.5 ? F("Открыто") : F("Закрыто"));
    }
    pos = fromPos + (target - fromPos) * easeInOut(t);
    writeServos();
  } else if (attached && (pos <= 0.001 || pos >= 0.999) && now - stoppedMs > HOLD_MS) {
    // в крайних положениях тяга и качалка стоят в линию и держат крышку сами,
    // сервоприводы можно отпустить: меньше гудят и тратят батарейки.
    // Если остановили кнопкой C посередине, держим: иначе половина упадёт.
    detachServos();
  }
}
