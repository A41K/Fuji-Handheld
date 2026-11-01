#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

const uint8_t PIN_UP    = 1;
const uint8_t PIN_DOWN  = 2;
const uint8_t PIN_LEFT  = 4;
const uint8_t PIN_RIGHT = 5;
const uint8_t PIN_A     = 6;
const uint8_t PIN_B     = 15;
const uint8_t PIN_VBAT  = 14;

const int SCREEN_W = 320;
const int SCREEN_H = 240;

inline bool btnUp()     { return !digitalRead(PIN_UP); }
inline bool btnDown()   { return !digitalRead(PIN_DOWN); }
inline bool btnLeft()   { return !digitalRead(PIN_LEFT); }
inline bool btnRight()  { return !digitalRead(PIN_RIGHT); }
inline bool btnA()      { return !digitalRead(PIN_A); }
inline bool btnB()      { return !digitalRead(PIN_B); }

inline bool consoleBegin(TFT_eSPI &tft, TFT_eSprite &frame) {
    const uint8_t pins[] = { PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_A, PIN_B };
    for (uint8_t i = 0; i < 6; i++) pinMode(pins[i], INPUT_PULLUP);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    frame.setColorDepth(16);
    return frame.createSprite(SCREEN_W, SCREEN_H) != nullptr;
}

inline float consoleBattery() {
    return analogReadMilliVolts(PIN_VBAT) * 2 / 1000.0f;

}

inline void returnToLauncher() {
    const esp_partition *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, null);
    if (p && esp_ota_set_boot_partition(p) == ESP_OK) esp_restart();
}

inline void consoleCheckExit() {
    static uint32_t since = 0;
    if (btnA() && btnB()) {
        if (!since) since = millis();
        else if (millis() - since > 1000) returnToLauncher();
    } else {
        since = 0;
    }
}