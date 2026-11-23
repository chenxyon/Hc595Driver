/**
 * @file     Hc595Driver.cpp
 * @brief    74HC595 串入并出移位寄存器驱动 — v2.0.0
 *
 * v2.0.0 核心改动：
 *   - 内部用 uint8_t 数组存储状态，支持无限级联
 *   - 旧 API 全部转调数组实现，完全兼容 v1.0.0
 *   - 流水灯模式也改用数组，支持超过 4 片
 */

#include "Hc595Driver.h"
#include <string.h>

// ============================================================
// 构造 / 析构
// ============================================================

Hc595Driver::Hc595Driver()
    : ds_pin_(0), shcp_pin_(0), stcp_pin_(0), num_chips_(0),
      initialized_(false),
      leds_(nullptr), led_count_(0), led_flow_idx_(0),
      flow_enabled_(false), flow_start_(0), flow_count_(0),
      flow_delay_ms_(0), flow_bounce_(false),
      flow_idx_(0), flow_dir_(1), flow_last_ms_(0)
{
    memset(current_output_, 0, sizeof(current_output_));
}

// ============================================================
// 底层时序（与 v1.0.0 完全相同）
// ============================================================

void Hc595Driver::shift_out_byte_(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        digitalWrite(ds_pin_, (data >> i) & 0x01 ? HIGH : LOW);
        delayMicroseconds(1);
        digitalWrite(shcp_pin_, HIGH);
        delayMicroseconds(1);
        digitalWrite(shcp_pin_, LOW);
    }
}

void Hc595Driver::latch_()
{
    digitalWrite(stcp_pin_, HIGH);
    delayMicroseconds(1);
    digitalWrite(stcp_pin_, LOW);
}

/**
 * ★ v2.0.0 核心函数：把 current_output_ 数组一次性发到所有芯片
 *
 * 关键：先发最后一片（index = num_chips_-1），再往前...最后发 U1（index = 0）
 *       因为 74HC595 是串行移位：
 *         第一个 byte → 推到最后一片（最远那片）
 *         第二个 byte → 推到倒数第二片
 *         ...
 *         最后一个 byte → 推到第一片（U1，最近 ESP32 那片）
 */
void Hc595Driver::refresh_hw_()
{
    if (!initialized_) return;
    // 从最后一片往前发
    for (int i = num_chips_ - 1; i >= 0; i--) {
        shift_out_byte_(current_output_[i]);
    }
    latch_();
}

// ============================================================
// 初始化
// ============================================================

int Hc595Driver::begin(int ds, int shcp, int stcp, int num_chips)
{
    if (num_chips < 1 || num_chips > HC595_MAX_CHIPS) return -1;

    ds_pin_    = ds;
    shcp_pin_  = shcp;
    stcp_pin_  = stcp;
    num_chips_ = num_chips;
    initialized_ = true;

    pinMode(ds,   OUTPUT);
    pinMode(shcp, OUTPUT);
    pinMode(stcp, OUTPUT);
    digitalWrite(ds,   LOW);
    digitalWrite(shcp, LOW);
    digitalWrite(stcp, LOW);

    memset(current_output_, 0, sizeof(current_output_));
    refresh_hw_();
    return 0;
}

// ============================================================
// ★ 新 API：数组方式（v2.0.0 新增，推荐用）
// ============================================================

void Hc595Driver::write_array(const uint8_t *arr, int count)
{
    if (!initialized_ || count < 1) return;
    // 超过实际片数的部分截断
    if (count > num_chips_) count = num_chips_;
    memcpy(current_output_, arr, count);
    // 多余的片保持灭
    if (count < num_chips_) {
        memset(current_output_ + count, 0, num_chips_ - count);
    }
    refresh_hw_();
}

void Hc595Driver::set_chip_q(int chip, int q)
{
    if (!initialized_) return;
    if (chip < 0 || chip >= num_chips_) return;
    if (q < 0 || q > 7) return;
    current_output_[chip] |= (1 << q);
    refresh_hw_();
}

void Hc595Driver::clr_chip_q(int chip, int q)
{
    if (!initialized_) return;
    if (chip < 0 || chip >= num_chips_) return;
    if (q < 0 || q > 7) return;
    current_output_[chip] &= ~(1 << q);
    refresh_hw_();
}

void Hc595Driver::toggle_chip_q(int chip, int q)
{
    if (!initialized_) return;
    if (chip < 0 || chip >= num_chips_) return;
    if (q < 0 || q > 7) return;
    current_output_[chip] ^= (1 << q);
    refresh_hw_();
}

uint8_t Hc595Driver::get_chip(int chip) const
{
    if (chip < 0 || chip >= num_chips_) return 0;
    return current_output_[chip];
}

// ============================================================
// ★ 旧 API：内部转调数组方式（v1.0.0 兼容，内部实现已改写）
// ============================================================

void Hc595Driver::write(uint32_t data)
{
    if (!initialized_) return;
    // 旧 API 最多处理 4 片（32bit），更多请用 write_array()
    int chips = (num_chips_ <= 4) ? num_chips_ : 4;
    for (int i = 0; i < chips; i++) {
        current_output_[i] = (data >> (i * 8)) & 0xFF;
    }
    // 超过 4 片的部分保持灭
    if (num_chips_ > 4) {
        memset(current_output_ + 4, 0, num_chips_ - 4);
    }
    refresh_hw_();
}

void Hc595Driver::set_bit(int bit)
{
    if (!initialized_) return;
    int chip = bit / 8;
    int q    = bit % 8;
    if (chip >= 0 && chip < num_chips_) {
        current_output_[chip] |= (1 << q);
        refresh_hw_();
    }
}

void Hc595Driver::clr_bit(int bit)
{
    if (!initialized_) return;
    int chip = bit / 8;
    int q    = bit % 8;
    if (chip >= 0 && chip < num_chips_) {
        current_output_[chip] &= ~(1 << q);
        refresh_hw_();
    }
}

void Hc595Driver::toggle_bit(int bit)
{
    if (!initialized_) return;
    int chip = bit / 8;
    int q    = bit % 8;
    if (chip >= 0 && chip < num_chips_) {
        current_output_[chip] ^= (1 << q);
        refresh_hw_();
    }
}

void Hc595Driver::clear()
{
    if (!initialized_) return;
    memset(current_output_, 0, num_chips_);
    refresh_hw_();
}

void Hc595Driver::all_on()
{
    if (!initialized_) return;
    memset(current_output_, 0xFF, num_chips_);
    refresh_hw_();
}

uint32_t Hc595Driver::get_output() const
{
    uint32_t result = 0;
    int chips = (num_chips_ <= 4) ? num_chips_ : 4;
    for (int i = 0; i < chips; i++) {
        result |= ((uint32_t)current_output_[i]) << (i * 8);
    }
    return result;
}

// ============================================================
// 流水灯模式（内部也改用数组实现，支持无限级联）
// ============================================================

void Hc595Driver::flow(int start_bit, int count, uint32_t delay_ms, bool bounce)
{
    if (!initialized_) return;
    if (count < 1 || start_bit < 0 || start_bit + count > 8 * num_chips_) return;

    flow_enabled_   = true;
    flow_start_     = start_bit;
    flow_count_     = count;
    flow_delay_ms_  = delay_ms;
    flow_bounce_    = bounce;
    flow_idx_       = 0;
    flow_dir_       = 1;
    flow_last_ms_   = 0;   // 首次立即刷新

    // 立即显示第一步
    clear();
    int bit = flow_start_ + flow_idx_;
    int chip = bit / 8;
    int q    = bit % 8;
    current_output_[chip] |= (1 << q);
    refresh_hw_();
    flow_last_ms_ = millis();
}

void Hc595Driver::flow_stop()
{
    flow_enabled_ = false;
    clear();
}

void Hc595Driver::loop()
{
    if (!flow_enabled_ || !initialized_) return;

    uint32_t now = millis();
    if (now - flow_last_ms_ < flow_delay_ms_) return;
    flow_last_ms_ = now;

    // 推进状态
    if (flow_bounce_) {
        int next = flow_idx_ + flow_dir_;
        if (next >= flow_count_ || next < 0) {
            flow_dir_ = -flow_dir_;
            next = flow_idx_ + flow_dir_;
        }
        flow_idx_ = next;
    } else {
        flow_idx_ = (flow_idx_ + 1) % flow_count_;
    }

    // ★ 用数组方式刷新（支持超过 4 片）
    memset(current_output_, 0, num_chips_);
    int bit = flow_start_ + flow_idx_;
    int chip = bit / 8;
    int q    = bit % 8;
    current_output_[chip] |= (1 << q);
    refresh_hw_();
}

// ============================================================
// ★ v2.0.0 新增：带 LedMap 的 begin() 重载 + 效果函数
// ============================================================

int Hc595Driver::begin(int ds, int shcp, int stcp, int num_chips,
                       const LedMap *leds, int led_count)
{
    int ret = begin(ds, shcp, stcp, num_chips);   // 调基础版 begin()
    if (ret != 0) return ret;
    leds_         = leds;
    led_count_    = led_count;
    led_flow_idx_ = 0;
    return 0;
}

int Hc595Driver::begin(const Hc595Config &cfg)
{
    return begin(cfg.ds, cfg.shcp, cfg.stcp, cfg.num_chips);
}

int Hc595Driver::begin(const Hc595Config &cfg, const LedMap *leds, int led_count)
{
    return begin(cfg.ds, cfg.shcp, cfg.stcp, cfg.num_chips, leds, led_count);
}

void Hc595Driver::self_test()
{
    if (!initialized_ || leds_ == nullptr) return;
    for (int i = 0; i < led_count_; i++) {
        set_chip_q(leds_[i].chip, leds_[i].q);
        Serial.printf("  LED%d (U%d.Q%d)\n", i + 1, leds_[i].chip + 1, leds_[i].q);
        delay(800);
        clear();
        delay(200);
    }
}

void Hc595Driver::water_flow()
{
    if (!initialized_ || leds_ == nullptr) return;
    set_chip_q(leds_[led_flow_idx_].chip, leds_[led_flow_idx_].q);
    Serial.printf("LED%d 亮 (U%d.Q%d)\n", led_flow_idx_ + 1, leds_[led_flow_idx_].chip + 1, leds_[led_flow_idx_].q);
    delay(500);
    clear();
    delay(200);
    led_flow_idx_ = (led_flow_idx_ + 1) % led_count_;
}

void Hc595Driver::led_on(int index)
{
    if (!initialized_ || leds_ == nullptr) return;
    if (index < 0 || index >= led_count_) return;
    set_chip_q(leds_[index].chip, leds_[index].q);
}

void Hc595Driver::led_off(int index)
{
    if (!initialized_ || leds_ == nullptr) return;
    if (index < 0 || index >= led_count_) return;
    clr_chip_q(leds_[index].chip, leds_[index].q);
}

#include "hc595driver_version.h"
#include "version_manager.h"
COMPONENT_VERSION_REGISTER(hc595_driver, HC595_DRIVER_VERSION_STRING)
