/**
 * @file     main.cpp
 * @brief    Hc595Driver 分组闪烁示例 —— 全亮全灭 + 奇偶交替
 *
 * 演示 write() 一次性写入多位状态，适合需要精确控制哪几个灯亮的场景。
 *
 * 接线：
 *   ESP32-S3 GPIO5  -> 74HC595 14脚 DS
 *   ESP32-S3 GPIO18 -> 74HC595 11脚 SHCP
 *   ESP32-S3 GPIO19 -> 74HC595 12脚 STCP
 */

#include <Arduino.h>
#include <Hc595Driver.h>

static const int DS_PIN   = 5;
static const int SHCP_PIN = 18;
static const int STCP_PIN = 19;

Hc595Driver led;

void setup()
{
    Serial.begin(115200);
    delay(200);

    Serial.println();
    Serial.println(F("=== Hc595Driver 分组闪烁示例 ==="));

    if (led.begin(DS_PIN, SHCP_PIN, STCP_PIN, /*num_chips=*/1) != 0) {
        Serial.println(F("初始化失败！"));
        while (1) { delay(1000); }
    }
}

void loop()
{
    // --- 效果 1：四灯同时闪 ---
    // bit1|bit2|bit3|bit4 = 0x1E = 00011110
    Serial.println(F("四灯同时亮"));
    led.write((1UL << 1) | (1UL << 2) | (1UL << 3) | (1UL << 4));
    delay(500);

    Serial.println(F("四灯全灭"));
    led.clear();
    delay(500);

    // --- 效果 2：Q1+Q3 与 Q2+Q4 交替闪 ---
    Serial.println(F("Q1+Q3 亮"));
    led.write((1UL << 1) | (1UL << 3));   // 0x0A
    delay(500);

    Serial.println(F("Q2+Q4 亮"));
    led.write((1UL << 2) | (1UL << 4));   // 0x14
    delay(500);
}
