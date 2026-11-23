/**
 * @file     main.cpp
 * @brief    Hc595Driver 基础示例 —— 单灯点灭 + 翻转
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
    Serial.println(F("=== Hc595Driver 基础示例：单灯点灭 ==="));

    if (led.begin(DS_PIN, SHCP_PIN, STCP_PIN, /*num_chips=*/1) != 0) {
        Serial.println(F("初始化失败！"));
        while (1) { delay(1000); }
    }
    Serial.println(F("初始化成功，Q1 开始闪烁..."));
}

void loop()
{
    // 方式 1：set_bit 单独点亮（其他位保持不变）
    led.set_bit(1);       // Q1 = bit1
    Serial.println(F("Q1 亮"));
    delay(1000);

    // 方式 2：clr_bit 单独灭掉
    led.clr_bit(1);
    Serial.println(F("Q1 灭"));
    delay(1000);

    // 方式 3：toggle_bit 翻转
    led.toggle_bit(1);
    Serial.println(F("Q1 翻转"));
    delay(1000);

    led.toggle_bit(1);    // 翻回去，保持对称
    delay(1000);
}
