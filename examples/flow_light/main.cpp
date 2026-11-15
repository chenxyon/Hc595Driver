/**
 * @file     main.cpp
 * @brief    Hc595Driver 流水灯示例 —— 单向 + 往返
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
    Serial.println(F("=== Hc595Driver 流水灯示例 ==="));

    if (led.begin(DS_PIN, SHCP_PIN, STCP_PIN, /*num_chips=*/1) != 0) {
        Serial.println(F("初始化失败！"));
        while (1) { delay(1000); }
    }

    // 启动单向流水灯：Q1 -> Q2 -> Q3 -> Q4，每步 500ms
    Serial.println(F("单向流水灯：Q1->Q2->Q3->Q4，每步 500ms"));
    led.flow(/*start_bit=*/1, /*count=*/4, /*delay_ms=*/500, /*bounce=*/false);
}

void loop()
{
    // 流水灯需要在 loop() 里持续驱动
    led.loop();
}
