// Jarvis indicator firmware v4 — write-only integration с оркестратором
//
// LED (GPIO2, единственный управляемый):
//   boot | idle | listening | thinking | speaking | guest | error
//
// Кнопка BOOT (GPIO0) — локальный mute:
//   короткое нажатие         -> toggle mute
//   когда mute=ON            -> LED показывает паттерн muted (короткий blip раз в 2.5 с)
//                               команды оркестратора запоминаются, но НЕ отрисовываются
//   когда mute=OFF           -> отрисовывается последняя команда оркестратора
//
// Длинное нажатие (>= 800 мс) — sync: принудительно перерисовать текущее состояние.
//
// Serial: 115200, 8N1. RX и TX работают надёжно на нормальном USB-порту
// (если TX плюётся мусором — попробовать другой физический порт компа,
// проблема не в плате).

#include <Arduino.h>

constexpr int LED_PIN     = 2;
constexpr int BUTTON_PIN  = 0;
constexpr int LEDC_CH     = 0;
constexpr int PWM_FREQ    = 5000;
constexpr int PWM_RES     = 8;
constexpr int PWM_MAX     = 255;

constexpr uint32_t DEBOUNCE_MS  = 30;
constexpr uint32_t LONGPRESS_MS = 800;

enum State : uint8_t {
  ST_BOOT = 0, ST_IDLE, ST_LISTENING, ST_THINKING,
  ST_SPEAKING, ST_GUEST, ST_ERROR,
};

State orchState  = ST_BOOT;   // что запросил оркестратор
uint32_t orchStartMs = 0;

bool mute = false;
uint32_t muteStartMs = 0;

String rxBuf;

bool     btnStableHigh   = true;
uint32_t btnLastChangeMs = 0;
uint32_t btnPressedAtMs  = 0;
bool     btnLongFired    = false;

static void writePwm(int v) {
  ledcWrite(LEDC_CH, constrain(v, 0, PWM_MAX));
}

static void applyOrchState(State s, const char* label) {
  orchState = s;
  orchStartMs = millis();
  Serial.print("OK "); Serial.println(label);
}

static void parseCommand(const String& cmd) {
  String c = cmd; c.trim(); c.toLowerCase();
  if (c.length() == 0) return;
  if (c == "ping")       { Serial.println("PONG"); return; }
  if (c == "boot")       { applyOrchState(ST_BOOT,      "boot");      return; }
  if (c == "idle")       { applyOrchState(ST_IDLE,      "idle");      return; }
  if (c == "listening")  { applyOrchState(ST_LISTENING, "listening"); return; }
  if (c == "thinking")   { applyOrchState(ST_THINKING,  "thinking");  return; }
  if (c == "speaking")   { applyOrchState(ST_SPEAKING,  "speaking");  return; }
  if (c == "guest")      { applyOrchState(ST_GUEST,     "guest");     return; }
  if (c == "error")      { applyOrchState(ST_ERROR,     "error");     return; }
  if (c == "unmute")     { mute = false; Serial.println("OK unmute"); return; }
  if (c == "mute")       { mute = true; muteStartMs = millis();
                           Serial.println("OK mute"); return; }
  Serial.print("ERR unknown "); Serial.println(c);
}

static int patternValue(State s, uint32_t t) {
  switch (s) {
    case ST_BOOT: {
      float phase = (t % 1000) / 1000.0f;
      return (int)((0.5f + 0.5f * sinf(phase * 2.0f * PI)) * PWM_MAX);
    }
    case ST_IDLE: {
      float phase = (t % 5000) / 5000.0f;
      return (int)((0.10f + 0.15f * (0.5f + 0.5f * sinf(phase * 2.0f * PI))) * PWM_MAX);
    }
    case ST_LISTENING: return PWM_MAX;
    case ST_THINKING: {
      float phase = (t % 500) / 500.0f;
      return (int)((0.30f + 0.70f * (0.5f + 0.5f * sinf(phase * 2.0f * PI))) * PWM_MAX);
    }
    case ST_SPEAKING: {
      uint32_t phase = (t / 125) % 2;
      return phase ? PWM_MAX : (int)(0.40f * PWM_MAX);
    }
    case ST_GUEST: {
      uint32_t p = t % 2000;
      return ((p < 80) || (p >= 200 && p < 280)) ? PWM_MAX : 0;
    }
    case ST_ERROR: {
      uint32_t phase = (t / 62) % 2;
      return phase ? PWM_MAX : 0;
    }
  }
  return 0;
}

static void applyPattern() {
  if (mute) {
    uint32_t t = millis() - muteStartMs;
    uint32_t p = t % 2500;
    writePwm((p < 60) ? (int)(0.6f * PWM_MAX) : 0);
  } else {
    uint32_t t = millis() - orchStartMs;
    writePwm(patternValue(orchState, t));
  }
}

static void pollButton() {
  bool raw = digitalRead(BUTTON_PIN) == HIGH;
  uint32_t now = millis();

  if (raw != btnStableHigh) {
    if (now - btnLastChangeMs >= DEBOUNCE_MS) {
      btnStableHigh   = raw;
      btnLastChangeMs = now;

      if (!btnStableHigh) {
        btnPressedAtMs = now;
        btnLongFired   = false;
      } else {
        uint32_t held = now - btnPressedAtMs;
        if (!btnLongFired && held >= DEBOUNCE_MS) {
          // short press: toggle mute локально
          mute = !mute;
          muteStartMs = now;
          Serial.print("EVT btn_short mute="); Serial.println(mute ? "on" : "off");
        }
      }
    }
  } else {
    btnLastChangeMs = now;
  }

  if (!btnStableHigh && !btnLongFired &&
      (now - btnPressedAtMs) >= LONGPRESS_MS) {
    btnLongFired = true;
    // long press: пока просто уведомление, поведение доопределит оркестратор
    Serial.println("EVT btn_long");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(120);
    digitalWrite(LED_PIN, LOW);  delay(120);
  }

  ledcSetup(LEDC_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(LED_PIN, LEDC_CH);

  orchState   = ST_BOOT;
  orchStartMs = millis();

  delay(100);
  Serial.println();
  Serial.println("READY");
  Serial.println("state=boot");
}

void loop() {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (rxBuf.length() > 0) { parseCommand(rxBuf); rxBuf = ""; }
    } else if (rxBuf.length() < 64) {
      rxBuf += ch;
    }
  }
  pollButton();
  applyPattern();
  delay(5);
}
