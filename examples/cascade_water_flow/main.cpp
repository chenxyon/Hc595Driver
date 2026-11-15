/**
 * @file     main.cpp
 * @brief    Hc595Driver v2.0.0 旗舰示例 —— 2片级联 + LedMap + 效果函数
 *
 * 本示例演示 v2.0.0 新增的全部核心特性：
 *   1. Hc595Config 结构体初始化（干净的配置方式）
 *   2. LedMap 映射表（代码与硬件解耦，流水顺序 = 数组顺序）
 *   3. self_test() 自检逐个点亮
 *   4. water_flow() LedMap 流水灯
 *
 * 接线（级联，前出后入，9接14）：
 *   ESP32-S3 GPIO5  -> U1 14脚 DS
 *   U1 9脚 QH'      -> U2 14脚 DS
 *   U2 9脚 QH'      -> 悬空（最后一片不用接）
 *
 *   ESP32-S3 GPIO18 -> 所有片 11脚 SHCP 并联（移位时钟）
 *   ESP32-S3 GPIO19 -> 所有片 12脚 STCP 并联（锁存时钟）
 *   所有片：VCC=3V3(16脚), GND=GND(8脚), OE=GND(13脚), SRCLR=3V3(10脚)
 *
 *   U1: Q1(1脚)->LED1, Q2(2脚)->LED2
 *   U2: Q1(1脚)->LED3, Q2(2脚)->LED4
 *
 * 效果：
 *   setup() 自检：LED1→LED2→LED3→LED4 逐个亮 800ms
 *   loop()  流水：LED1→LED2→LED3→LED4 循环亮 500ms/步
 */

#include <Arduino.h>
#include <Hc595Driver.h>    // ★ Hc595Config + LedMap + Hc595Driver 都在这一个头文件

// ============================================================
// ★ 配置区 —— 只改这里就能适配任何硬件
// ============================================================

// 硬件配置（用结构体，最干净）
static const Hc595Config k_cfg = {
    .ds         = 5,     // 串行数据 → U1.DS(14脚)
    .shcp       = 18,    // 移位时钟 → 所有片.SHCP(11脚) 并联
    .stcp       = 19,    // 锁存时钟 → 所有片.STCP(12脚) 并联
    .num_chips  = 2      // 级联片数
};

// LED 映射表（流水顺序 = 数组顺序）
static const LedMap k_leds[] = {
    {0, 1},   // LED1 → U1.Q1
    {0, 2},   // LED2 → U1.Q2
    {1, 1},   // LED3 → U2.Q1
    {1, 2},   // LED4 → U2.Q2
};
static const int k_led_count = sizeof(k_leds) / sizeof(k_leds[0]);

// ============================================================
// 74HC595 控制器（一个类搞定：底层 API + LedMap 效果函数）
// ============================================================
Hc595Driver led;

// ============================================================
// Arduino 入口
// ============================================================

void setup()
{
    Serial.begin(115200);
    delay(200);

    Serial.println();
    Serial.printf("=== Hc595Driver v2.0.0 旗舰示例：%d片级联 + LedMap ===\n", k_cfg.num_chips);
    Serial.printf("LED 映射：");
    for (int i = 0; i < k_led_count; i++) {
        Serial.printf("LED%d=U%d.Q%d  ", i + 1, k_leds[i].chip + 1, k_leds[i].q);
    }
    Serial.println();

    // 初始化（★ 用 Hc595Config 结构体 + LedMap，一行搞定）
    led.begin(k_cfg, k_leds, k_led_count);

    if (!led.is_ready()) {
        Serial.println("初始化失败！");
        while (1) delay(1000);
    }
    Serial.printf("初始化成功（%d片，%d个LED）\n", led.chip_count(), led.led_count());

    // 自检 + 流水灯
    Serial.println("自检：逐个点亮 LED1~LED4，每个 800ms");
    led.self_test();
    Serial.println("自检完成，启动流水灯...");
}

void loop()
{
    led.water_flow();
}
