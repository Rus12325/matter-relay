#include <Arduino.h>
#include <Network.h>   // <--- ДОБАВИТЬ ЭТУ СТРОКУ СЮДА!
#include <Matter.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

// ===============================
// Wi-Fi
// ===============================

#include "secrets.h"
const char *ssid     = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// ===============================
// Пины
// ===============================

// Для ESP32-C6 Super Mini лучше не использовать GPIO2/GPIO3 без проверки.
// Если можешь, перенеси реле на GPIO4/GPIO5 или другие безопасные GPIO.
const int RELAY1_PIN = 2;   // импульсное реле / кнопка ПК
const int RELAY2_PIN = 3;   // обычное реле
const int BUTTON_PIN = 9;   // кнопка BOOT

// Active-low модули: LOW = включено, HIGH = выключено
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

const int PULSE_MS = 500;

// Matter device type:
// 0x0100 = On/Off Light
// 0x010A = On/Off Plug-in Unit
const uint16_t MATTER_ON_OFF_PLUG_IN_UNIT = 0x010A;

// =====================================================
// ВКЛЮЧИ ТОЛЬКО ОДИН из вариантов, если библиотека
// поддерживает смену device type.
//
// Если раскомментировать неправильный вариант,
// будет ошибка компиляции.
// =====================================================

// Вариант 1: тип устройства передаётся в конструктор
// #define DEVICE_TYPE_IN_CONSTRUCTOR

// Вариант 2: тип устройства передаётся в begin()
// #define DEVICE_TYPE_IN_BEGIN

// Вариант 3: тип устройства задаётся методом setDeviceType()
// #define DEVICE_TYPE_SET_METHOD

#if defined(DEVICE_TYPE_IN_CONSTRUCTOR)
MatterOnOffPlugin relay1(MATTER_ON_OFF_PLUG_IN_UNIT);
MatterOnOffPlugin relay2(MATTER_ON_OFF_PLUG_IN_UNIT);
#else
MatterOnOffPlugin relay1;
MatterOnOffPlugin relay2;
#endif

Preferences prefs;

volatile bool pulseRequest = false;
volatile bool matterReady = false;

// ===============================
// Обработчики Matter
// ===============================

// Реле 1: любое переключение тумблера в приложении = импульс
bool setRelay1(bool state) {
  if (!matterReady) return true;

  pulseRequest = true;

  // Если библиотека поддерживает явное обновление состояния,
  // после импульса желательно вернуть состояние в false.
  // Например, если есть метод setState():
  //
  // relay1.setState(false);

  return true;
}

// Реле 2: обычное реле с сохранением состояния
bool setRelay2(bool state) {
  if (!matterReady) return true;

  digitalWrite(RELAY2_PIN, state ? RELAY_ON : RELAY_OFF);
  prefs.putBool("r2", state);

  Serial.printf("Реле 2: %s\n", state ? "ВКЛ" : "ВЫКЛ");
  return true;
}

// ===============================
// Вспомогательные функции
// ===============================

void syncTime() {
  // Синхронизация времени через NTP.
  // Убирает/уменьшает ошибки вида:
  // The device does not support GetClock_RealTimeMS()
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

// ===============================
// setup
// ===============================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== ESP32-C6 Matter Relay PlatformIO ===");

  prefs.begin("relays", false);
  bool r2 = prefs.getBool("r2", false);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Импульсное реле стартует разомкнутым
  digitalWrite(RELAY1_PIN, RELAY_OFF);

  // Обычное реле восстанавливаем из памяти
  digitalWrite(RELAY2_PIN, r2 ? RELAY_ON : RELAY_OFF);

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("matter-relay-c6");
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // Раскомментируй, если поддерживается ядром:
  // WiFi.setCountryCode("RU");
  // WiFi.enableIpV6();

  Serial.printf("Подключение к Wi-Fi: %s\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long wifiStart = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWi-Fi подключен, IP: %s\n", WiFi.localIP().toString().c_str());

    // Если включил IPv6 выше, можно раскомментировать:
    // Serial.printf("IPv6: %s\n", WiFi.localIPv6().toString().c_str());

    // Если питание слабое, можно снизить мощность Wi-Fi.
    // Если плата далеко от роутера — убери эту строку.
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    syncTime();
  } else {
    Serial.println("\nWi-Fi не подключился. Будем пробовать дальше.");
    WiFi.printDiag(Serial);
  }

  // ===============================
  // Matter endpoints
  // ===============================

#if defined(DEVICE_TYPE_SET_METHOD)
  relay1.setDeviceType(MATTER_ON_OFF_PLUG_IN_UNIT);
  relay2.setDeviceType(MATTER_ON_OFF_PLUG_IN_UNIT);
#endif

#if defined(DEVICE_TYPE_IN_BEGIN)
  relay1.begin(false, MATTER_ON_OFF_PLUG_IN_UNIT);
  relay2.begin(r2, MATTER_ON_OFF_PLUG_IN_UNIT);
#else
  relay1.begin(false);
  relay2.begin(r2);
#endif

  relay1.onChange(setRelay1);
  relay2.onChange(setRelay2);

  Matter.begin();
  matterReady = true;

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("=== Устройство не сопряжено. Данные для добавления: ===");
    Serial.printf("Ручной код: %s\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  } else {
    Serial.println("Устройство уже сопряжено с Matter-контроллером");
  }
}

// ===============================
// loop
// ===============================

void loop() {
  // Периодическая попытка переподключить Wi-Fi
  static unsigned long lastWifiCheck = 0;

  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    Serial.println("Wi-Fi пропал, переподключаюсь...");
    WiFi.begin(ssid, password);
  }

  // Импульс реле 1 без blocking delay()
  static bool pulsing = false;
  static unsigned long pulseStart = 0;

  if (pulseRequest) {
    pulseRequest = false;
    pulsing = true;
    pulseStart = millis();

    digitalWrite(RELAY1_PIN, RELAY_ON);
    Serial.println("Импульс: контакты замкнуты");
  }

  if (pulsing && millis() - pulseStart >= PULSE_MS) {
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    pulsing = false;

    Serial.println("Импульс: контакты разомкнуты");

    // Если нужно, чтобы реле1 в Matter всегда оставалось "выключено",
    // после импульса можно вернуть состояние false, если API позволяет:
    //
    // relay1.setState(false);
  }

  // Кнопка BOOT
  static bool btnHeld = false;
  static unsigned long btnDownAt = 0;
  static bool longPressDone = false;

  bool btn = (digitalRead(BUTTON_PIN) == LOW);

  if (btn && !btnHeld) {
    btnHeld = true;
    btnDownAt = millis();
    longPressDone = false;
  }

  // Длинное нажатие 5 секунд — сброс Matter
  if (btn && btnHeld && !longPressDone && millis() - btnDownAt > 5000) {
    longPressDone = true;
    Serial.println("Долгое нажатие: сброс Matter-сопряжения...");

    Matter.decommission();
    delay(500);
    ESP.restart();
  }

  // Короткое нажатие — тестовый импульс
  if (!btn && btnHeld) {
    btnHeld = false;

    if (!longPressDone && millis() - btnDownAt > 50) {
      Serial.println("Кнопка BOOT: тестовый импульс");
      pulseRequest = true;
    }
  }

  delay(10);
}