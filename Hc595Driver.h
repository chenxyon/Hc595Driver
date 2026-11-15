/**
 * @file     Hc595Driver.h
 * @brief    74HC595 串入并出移位寄存器驱动（Arduino 框架）
 * @version  v2.0.0
 *
 * v2.0.0 更新：
 *   - 新增数组方式 API（write_array / set_chip_q / clr_chip_q / toggle_chip_q）
 *   - 内部改用 uint8_t 数组，支持无限片级联（MAX_CHIPS=32，可改）
 *   - 保留 v1.0.0 旧 API（write(uint32_t) / set_bit / clr_bit ...），完全兼容
 *
 * 接线约定：
 *   DS    -> ESP32 GPIO（串行数据，只接第一片）
 *   SHCP  -> ESP32 GPIO（移位时钟，所有片并联）
 *   STCP  -> ESP32 GPIO（锁存时钟，所有片并联）
 *   OE    -> GND（低电平输出使能）
 *   SRCLR -> 3V3（高电平正常工作）
 *   QH'   -> 下一片的 DS（级联串联：前一片 QH' 接后一片 DS）
 *
 * 用法示例（新 API，推荐）：
 *   Hc595Driver led;
 *   led.begin(5, 18, 19, 3);              // DS=GPIO5, SHCP=GPIO18, STCP=GPIO19, 3片
 *   uint8_t chips[3] = {0};               // chips[0]=U1, chips[1]=U2, chips[2]=U3
 *   chips[0] |= (1 << 1);                 // U1.Q1 亮（bit1 = Q1）
 *   chips[2] |= (1 << 2);                 // U3.Q2 亮
 *   led.write_array(chips, 3);
 *   led.set_chip_q(0, 1);                 // 直接点 U1.Q1
 *   led.clr_chip_q(2, 3);                 // 直接灭 U3.Q3
 *
 * 用法示例（旧 API，v1.0.0 兼容）：
 *   led.write(0x0001);                    // 低8bit=U1, 次8bit=U2
 *   led.set_bit(9);                       // 全局位编号：bit9 = U2.Q1
 *   led.clear();
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#define HC595_MAX_CHIPS 32    // 最多支持 32 片 = 256 bit

class Hc595Driver {
public:
    Hc595Driver();

    /**
     * @brief  初始化驱动
     * @param  ds         串行数据引脚（ESP32 → U1.DS）
     * @param  shcp       移位时钟引脚（ESP32 → 所有片.SHCP）
     * @param  stcp       锁存时钟引脚（ESP32 → 所有片.STCP）
     * @param  num_chips  级联片数（1~HC595_MAX_CHIPS）
     * @return 0=成功，-1=参数错误
     */
    int begin(int ds, int shcp, int stcp, int num_chips = 1);

    // ============================================================
    // ★ 新 API（v2.0.0 数组方式，推荐用）
    // ============================================================

    /**
     * @brief  用数组写入所有芯片状态（最直观）
     * @param  arr   数组指针：arr[0]=U1, arr[1]=U2, ...
     *               每 bit 对应一个 Q：bit0=Q1, bit1=Q2, ...bit7=Q0
     * @param  count 需要发送的片数（≤ begin() 时指定的 num_chips）
     *
     * 用法示例：
     *   uint8_t chips[3] = {0};      // 3 片全灭
     *   chips[0] |= (1 << 1);        // U1.Q1 亮（bit1 = Q1）
     *   chips[2] |= (1 << 2);        // U3.Q2 亮
     *   led.write_array(chips, 3);
     */
    void write_array(const uint8_t *arr, int count);

    /**
     * @brief  直接设置某片某 Q 脚（其他保持不变）
     * @param  chip  片序号：0=U1, 1=U2, 2=U3, ...
     * @param  q     Q 脚：1=Q1, 2=Q2, ...7=Q7, 0=Q0
     */
    void set_chip_q(int chip, int q);

    /**
     * @brief  直接灭掉某片某 Q 脚（其他保持不变）
     */
    void clr_chip_q(int chip, int q);

    /**
     * @brief  翻转某片某 Q 脚
     */
    void toggle_chip_q(int chip, int q);

    /**
     * @brief  读取某片当前状态
     * @return 该片的 8bit 输出值
     */
    uint8_t get_chip(int chip) const;

    // ============================================================
    // ★ 旧 API（v1.0.0 保留，完全兼容，内部转调数组方式）
    // ============================================================

    /**
     * @brief  写入完整状态（v1.0.0 兼容）
     * @param  data  低 8bit=U1, 次 8bit=U2, ...以此类推
     *
     * 注意：仅支持 ≤4 片（32bit），更多片请用 write_array()
     */
    void write(uint32_t data);

    /**
     * @brief  单独点亮某一位（v1.0.0 兼容）
     * @param  bit  全局位编号：bit0=U1.Q1, bit1=U1.Q2, ...bit8=U2.Q1, ...
     */
    void set_bit(int bit);

    /**
     * @brief  单独灭掉某一位（v1.0.0 兼容）
     */
    void clr_bit(int bit);

    /**
     * @brief  翻转某一位（v1.0.0 兼容）
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
     * @brief  读取当前输出状态（v1.0.0 兼容，仅返回前 32bit）
     */
    uint32_t get_output() const;

    /**
     * @brief  是否初始化成功
     */
    bool is_ready() const { return initialized_; }

    // ============================================================
    // 流水灯模式
    // ============================================================

    /**
     * @brief  启动/设置流水灯模式（v1.0.0 兼容）
     * @param  start_bit  起始位（如 1 = 从 U1.Q1 开始）
     * @param  count      灯数量（如 6 = 6 个灯）
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

    // ★ v2.0.0：用数组存储所有片的状态（支持无限级联）
    uint8_t current_output_[HC595_MAX_CHIPS];

    // === 流水灯状态机 ===
    bool     flow_enabled_;
    int      flow_start_;
    int      flow_count_;
    uint32_t flow_delay_ms_;
    bool     flow_bounce_;
    int      flow_idx_;       // 当前第几步
    int      flow_dir_;       // 1=正向，-1=反向
    uint32_t flow_last_ms_;   // 上次更新时间

    // === 底层函数 ===
    void shift_out_byte_(uint8_t data);
    void latch_();
    void refresh_hw_();       // ★ v2.0.0：把 current_output_ 数组一次性发到硬件
};
