/**
 * @file     Hc595Driver.cpp
 * @brief    74HC595 驱动实现
 */

#include "Hc595Driver.h"

Hc595Driver::Hc595Driver()
    : ds_pin_(0), shcp_pin_(0), stcp_pin_(0), num_chips_(0),
      initialized_(false), current_output_(0),
      flow_enabled_(false), flow_start_(0), flow_count_(0),
      flow_delay_ms_(0), flow_bounce_(false),
      flow_idx_(0), flow_dir_(1), flow_last_ms_(0) {}

// ============================================================
// 底层时序
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

// ============================================================
// 初始化
// ============================================================

int Hc595Driver::begin(int ds, int shcp, int stcp, int num_chips)
{
    if (num_chips < 1 || num_chips > 4) return -1;

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

    current_output_ = 0;
    for (int i = 0; i < num_chips; i++) shift_out_byte_(0);
    latch_();
    return 0;
}

// ============================================================
// 基础写操作
// ============================================================

void Hc595Driver::write(uint32_t data)
{
    if (!initialized_) return;
    current_output_ = data;
    for (int i = num_chips_ - 1; i >= 0; i--) {
        shift_out_byte_((data >> (i * 8)) & 0xFF);
    }
    latch_();
}

void Hc595Driver::set_bit(int bit)
{
    write(current_output_ | (1UL << bit));
}

void Hc595Driver::clr_bit(int bit)
{
    write(current_output_ & ~(1UL << bit));
}

void Hc595Driver::toggle_bit(int bit)
{
    write(current_output_ ^ (1UL << bit));
}

void Hc595Driver::clear()
{
    write(0);
}

void Hc595Driver::all_on()
{
    // 根据片数决定全亮值
    uint32_t mask = (1UL << (8 * num_chips_)) - 1;
    write(mask);
}

// ============================================================
// 流水灯模式
// ============================================================

void Hc595Driver::flow(int start_bit, int count, uint32_t delay_ms, bool bounce)
{
    if (count < 1 || start_bit < 0 || start_bit + count > 8 * num_chips_) return;

    flow_enabled_   = true;
    flow_start_     = start_bit;
    flow_count_     = count;
    flow_delay_ms_  = delay_ms;
    flow_bounce_    = bounce;
    flow_idx_       = 0;
    flow_dir_       = 1;
    flow_last_ms_   = 0;  // 首次立即刷新

    // 立即显示第一步
    if (initialized_) {
        uint32_t mask = 1UL << (flow_start_ + flow_idx_);
        write(mask);
        flow_last_ms_ = millis();
    }
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

    // 输出
    uint32_t mask = 1UL << (flow_start_ + flow_idx_);
    write(mask);
}
