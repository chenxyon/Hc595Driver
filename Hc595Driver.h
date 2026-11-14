/**
 * @file     Hc595Driver.h
 * @brief    74HC595 串入并出移位寄存器驱动
 *
 * 接线约定：
 *   DS    -> ESP32 GPIO（串行数据）
 *   SHCP  -> ESP32 GPIO（移位时钟，上升沿移入）
 *   STCP  -> ESP32 GPIO（锁存时钟，上升沿输出）
 *   OE    -> GND 或 PWM 引脚（低电平有效，低=输出使能）
 *   SRCLR -> 3V3（高电平正常工作）
 *
 *   Q 输出 -> LED/继电器/... -> GND（高电平点亮）
 *
 * 用法示例：
 *   Hc595Driver led;
 *   led.begin(5, 18, 19, 1);         // DS=GPIO5, SHCP=GPIO18, STCP=GPIO19, 1片
 *   led.write(0xFF);                  // 全亮
 *   led.set_bit(3);                   // 单独点 Q3
 *   led.clr_bit(3);                   // 单独灭 Q3
 *   led.flow(1, 4, 500, false);       // Q1->Q2->Q3->Q4 单向流水
 *   void loop() { led.loop(); }       // 流水灯需要 loop() 驱动
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

class Hc595Driver {
public:
    Hc595Driver();

    /**
     * @brief  初始化驱动
     * @param  ds         串行数据引脚
     * @param  shcp       移位时钟引脚
     * @param  stcp       锁存时钟引脚
     * @param  num_chips  级联片数（1~4）
     * @return 0=成功，-1=参数错误
     */
    int begin(int ds, int shcp, int stcp, int num_chips = 1);

    /**
     * @brief  写入完整状态（所有位一起设置）
     * @param  data  要写入的值（bit0=Q0, bit1=Q1, ...）
     */
    void write(uint32_t data);

    /**
     * @brief  单独点亮某一位（其他位保持不变）
     * @param  bit  目标位编号（0~7*num_chips-1）
     */
    void set_bit(int bit);

    /**
     * @brief  单独灭掉某一位（其他位保持不变）
     * @param  bit  目标位编号
     */
    void clr_bit(int bit);

    /**
     * @brief  翻转某一位
     * @param  bit  目标位编号
     */
    void toggle_bit(int bit);

    /**
     * @brief  全灭
     */
    void clear();

    /**
     * @brief  全亮
     */
    void all_on();

    /**
     * @brief  读取当前输出状态
     * @return 内部锁存寄存器值
     */
    uint32_t get_output() const { return current_output_; }

    /**
     * @brief  是否初始化成功
     */
    bool is_ready() const { return initialized_; }

    /**
     * @brief  启动/设置流水灯模式
     * @param  start_bit  起始位（如 1 = 从 Q1 开始）
     * @param  count      灯数量（如 4 = 4 个灯）
     * @param  delay_ms   每步间隔
     * @param  bounce     false=单向循环，true=往返
     */
    void flow(int start_bit, int count, uint32_t delay_ms, bool bounce = false);

    /**
     * @brief  停止流水灯模式
     */
    void flow_stop();

    /**
     * @brief  流水灯驱动（必须在 loop() 里调用）
     */
    void loop();

private:
    // === 硬件相关 ===
    int ds_pin_;
    int shcp_pin_;
    int stcp_pin_;
    int num_chips_;
    bool initialized_;
    uint32_t current_output_;

    // === 流水灯状态机 ===
    bool     flow_enabled_;
    int      flow_start_;
    int      flow_count_;
    uint32_t flow_delay_ms_;
    bool     flow_bounce_;
    int      flow_idx_;     // 当前第几步
    int      flow_dir_;     // 1=正向，-1=反向
    uint32_t flow_last_ms_; // 上次更新时间

    // === 内部函数 ===
    void shift_out_byte_(uint8_t data);
    void latch_();
};
