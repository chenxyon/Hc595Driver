/**
 * @file     Hc595Driver.h
 * @brief    74HC595 串入并出移位寄存器驱动（Arduino 框架）
 * @version  v2.0.0
 *
 * ============================================================================
 * 一、芯片简介
 * ============================================================================
 *   74HC595 是一款 8 位串行输入、并行输出的移位寄存器。
 *   简单说：ESP32 只需 3 个 GPIO，就能控制 N 片 595 的 N×8 个输出脚。
 *
 *   核心原理：
 *     DS   → 串行数据输入（每来一个 SHCP 时钟沿，数据移入内部寄存器）
 *     SHCP → 移位时钟（上升沿有效，内部寄存器整体后移一位）
 *     STCP → 锁存时钟（上升沿有效，把内部寄存器的数据锁存到输出）
 *     QH'  → 级联溢出输出（把最后一位数据传给下一片 595 的 DS）
 *     OE   → 输出使能（低电平=输出有效，高电平=输出高阻态）
 *     SRCLR→ 移位寄存器清零（低电平=强制清零，高电平=正常工作）
 *     Q1~Q0 → 并行输出引脚（NXP 命名：Q1=引脚1，Q2=引脚2，... Q0=引脚15）
 *
 * ============================================================================
 * 二、级联接线（多片串联）
 * ============================================================================
 *
 *   规则：前出后入，9接14（前一片 QH' 接后一片 DS）
 *
 *   串联逻辑：DS→[bit1][bit2]...[bit8]→QH'   每片内部结构
 *              Q1    Q2        Q0
 *
 *   多片级联（3 片示例）：
 *     ESP32.GPIO5 ──→ U1.DS(14脚)
 *     U1.QH'(9脚) ──→ U2.DS(14脚)
 *     U2.QH'(9脚) ──→ U3.DS(14脚)
 *     U3.QH'(9脚)    悬空（最后一片不用接）
 *
 *     公共信号并联：
 *     ESP32.GPIO18 ──→ U1.SHCP(11) + U2.SHCP(11) + U3.SHCP(11)
 *     ESP32.GPIO19 ──→ U1.STCP(12) + U2.STCP(12) + U3.STCP(12)
 *     3V3          ──→ 所有片 VCC(16脚)
 *     GND          ──→ 所有片 GND(8脚) + OE(13脚)
 *     3V3          ──→ 所有片 SRCLR(10脚)
 *
 *   数据发送顺序（非常重要！）：
 *     先发最后一片（U3）的 8bit → 再发中间片（U2）的 8bit → 最后发第一片（U1）的 8bit
 *     因为移位寄存器是串行推挤：先到的数据会被推到"更远"的芯片
 *
 * ============================================================================
 * 三、版本历史
 * ============================================================================
 *   v1.0.0 - 初始版本，uint32_t 存储（最多 4 片 = 32bit），旧 API（write/set_bit/...）
 *   v2.0.0 - 数组存储（支持 32 片 = 256bit），新增 Hc595Config/LedMap/效果函数
 *            完全向后兼容 v1.0.0（旧 API 全部保留）
 *
 * ============================================================================
 * 四、用法示例
 * ============================================================================
 *
 *   // ===== 方式 A：最简单的旧 API（v1.0.0 兼容）=====
 *   Hc595Driver drv;
 *   drv.begin(5, 18, 19, 2);           // DS=GPIO5, SHCP=GPIO18, STCP=GPIO19, 2片
 *   drv.write(0x0102);                  // 低8bit=U1(0x02→Q2亮), 次8bit=U2(0x01→Q1亮)
 *   drv.set_bit(9);                     // 全局位9 = U2.Q1 亮
 *   drv.flow(1, 6, 500); drv.loop();    // 流水灯（bit 1~6，500ms/步）
 *
 *   // ===== 方式 B：新 API（数组方式，推荐给芯片级操作）=====
 *   Hc595Driver drv;
 *   drv.begin(5, 18, 19, 3);
 *   uint8_t chips[3] = {0};
 *   chips[0] |= (1 << 1);               // U1.Q1 亮（bit1 = Q1）
 *   chips[2] |= (1 << 2);               // U3.Q2 亮
 *   drv.write_array(chips, 3);
 *   drv.set_chip_q(0, 1);               // 直接点 U1.Q1（内部也调 write_array）
 *
 *   // ===== 方式 C：Hc595Config + LedMap + 效果函数（v2.0.0 最推荐）=====
 *   Hc595Config cfg = {
 *       .ds = 5, .shcp = 18, .stcp = 19, .num_chips = 2
 *   };
 *   static const LedMap k_leds[] = {
 *       {0, 1},   // LED1 → U1.Q1  （chip=第几片，q=哪个Q脚）
 *       {0, 2},   // LED2 → U1.Q2
 *       {1, 1},   // LED3 → U2.Q1
 *       {1, 2},   // LED4 → U2.Q2
 *   };
 *   Hc595Driver led;
 *   led.begin(cfg, k_leds, 4);          // 结构体 + LedMap，一行搞定
 *   led.self_test();                     // 按 k_leds 顺序逐个亮（800ms/步）
 *   led.water_flow();                    // 流水灯（500ms/步，loop 里调）
 *
 * ============================================================================
 */

#pragma once

#include <Arduino.h>       // Arduino 框架（digitalWrite, pinMode, delay, Serial）
#include <stdint.h>        // uint8_t, uint32_t 等固定宽度类型
#include <string.h>        // memset, memcpy 等内存操作

/**
 * @brief  最大支持的级联片数
 *
 *   内部用 uint8_t 数组存储每片的状态，数组大小由这个宏决定。
 *   32 片 = 256 bit，对于 ESP32-S3 来说 RAM 完全够用（256 字节）。
 *   如果还要更多，改这个宏就行（比如 HC595_MAX_CHIPS 64）。
 *
 *   硬件上没有硬限制，理论上可以接无限多片（但受时钟速度和信号衰减影响）。
 */
#define HC595_MAX_CHIPS 32

// ============================================================================
// 结构体定义
// ============================================================================

/**
 * @brief  74HC595 硬件配置（v2.0.0 新增）
 *
 *   把 4 个初始化参数打包成一个结构体，让 begin() 签名更干净。
 *   以后如果需要加新参数（比如 SPI 模式、输出极性等），只要改这个结构体就行，
 *   不用改所有 begin() 重载的函数签名。
 *
 * 用法：
 *   Hc595Config cfg = {
 *       .ds        = 5,       // C++11  designated initializer，按名字赋值很清楚
 *       .shcp      = 18,
 *       .stcp      = 19,
 *       .num_chips = 2
 *   };
 *   led.begin(cfg);                                   // 不带 LedMap，只用底层 API
 *   led.begin(cfg, k_leds, k_led_count);              // 带 LedMap，能用效果函数
 */
struct Hc595Config {
    int ds;         // 串行数据引脚：ESP32 → U1.DS(14脚)
    int shcp;       // 移位时钟引脚：ESP32 → 所有片.SHCP(11脚) 并联
    int stcp;       // 锁存时钟引脚：ESP32 → 所有片.STCP(12脚) 并联
    int num_chips;  // 级联片数：1 ~ HC595_MAX_CHIPS（1=单片，2=两片串联...）
};

/**
 * @brief  LED 映射条目（v2.0.0 新增）
 *
 *   描述一个 LED 接在第几片 74HC595 的哪个 Q 脚上。
 *   LedMap 数组按顺序排列 = 流水灯的点亮顺序。
 *
 * 约定：
 *   chip: 第几片 74HC595（0=U1主片, 1=U2次片, 2=U3末片 ...）
 *         级联顺序：U1（最近 ESP32）→ U2 → U3（最远）
 *   q:    哪个 Q 脚（1=Q1引脚1, 2=Q2引脚2, ... 7=Q7引脚7, 0=Q0引脚15）
 *
 * 注意：74HC595N DIP-16 封装的引脚编号（容易搞混！）：
 *     引脚号 = 1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
 *     功能名 = Q1 Q2 Q3 Q4 Q5 Q6 Q7 GND QH SRCLR SHCP STCP OE DS Q0 VCC
 *   也就是说：Q1=引脚1（不是引脚15！），Q0=引脚15
 *
 * 用法示例：
 *   static const LedMap k_leds[] = {
 *       {0, 1},   // LED1 → U1.Q1（引脚1）  chip=0表示U1，q=1表示Q1
 *       {0, 2},   // LED2 → U1.Q2（引脚2）
 *       {1, 1},   // LED3 → U2.Q1（引脚1）
 *       {1, 2},   // LED4 → U2.Q2（引脚2）
 *   };
 */
struct LedMap {
    int chip;   // 片序号：0=U1, 1=U2, 2=U3 ...
    int q;      // Q 脚编号：1=Q1, 2=Q2, ... 7=Q7, 0=Q0
};

// ============================================================================
// Hc595Driver 类（核心驱动）
// ============================================================================

/**
 * @brief  74HC595 级联驱动（v2.0.0，向后兼容 v1.0.0）
 *
 * 功能分 3 层：
 *    1. 底层：shift_out_byte_() / latch_() / refresh_hw_() —— 直接操作 GPIO
 *    2. 中层：write_array() / set_chip_q() / clear()       —— 操作内部状态数组
 *    3. 上层：self_test() / water_flow() / led_on()        —— 效果函数（需要 LedMap）
 *
 * 内部状态：
 *    current_output_[32] = 每片 74HC595 当前的输出状态
 *    调用 set_chip_q() / clear() 等只修改这个数组，
 *    然后 refresh_hw_() 一次性把数组内容发到硬件。
 *    这样做的好处：可以多次修改数组，最后统一锁存一次，避免闪烁。
 *
 * 两种使用模式：
 *    - 不带 LedMap：只能用底层 API（write_array / set_chip_q / clear ...）
 *    - 带 LedMap：额外能用效果函数（self_test / water_flow / led_on）
 */
class Hc595Driver {
public:
    /**
     * @brief  构造函数
     *
     *   只做变量初始化，不碰硬件。
     *   真正的 GPIO 配置在 begin() 里做。
     */
    Hc595Driver();

    // ------------------------------------------------------------------------
    // 初始化（begin() 有 4 个重载，任选一个）
    // ------------------------------------------------------------------------

    /**
     * @brief  初始化驱动（基础版，v1.0.0 兼容）
     *
     *   初始化后，只能用底层 API（write_array / set_chip_q / clear ...）。
     *   想用 self_test() / water_flow() 等效果函数，请用带 LedMap 的 begin()。
     *
     * @param  ds         串行数据引脚：ESP32 → U1.DS(14脚)
     * @param  shcp       移位时钟引脚：ESP32 → 所有片.SHCP(11脚) 并联
     * @param  stcp       锁存时钟引脚：ESP32 → 所有片.STCP(12脚) 并联
     * @param  num_chips  级联片数：1~HC595_MAX_CHIPS，默认 1
     * @return 0=成功，-1=参数错误（num_chips 越界）
     */
    int begin(int ds, int shcp, int stcp, int num_chips = 1);

    /**
     * @brief  初始化（带 LedMap，v2.0.0 新增）
     *
     *   基础版 + LED 映射表。初始化后能用 self_test() / water_flow() / led_on()。
     *   内部先调基础版 begin()，再保存 LedMap 指针。
     *
     * @param  ds, shcp, stcp, num_chips  同基础版 begin()
     * @param  leds       LED 映射表数组（指针，外部必须保证生命周期够长）
     * @param  led_count  LED 数量（leds 数组的元素个数）
     */
    int begin(int ds, int shcp, int stcp, int num_chips,
              const LedMap *leds, int led_count);

    /**
     * @brief  初始化（用 Hc595Config 结构体，v2.0.0 新增，推荐）
     *
     *   和基础版功能完全相同，只是参数打包进了结构体。
     *   好处：调用点更干净，以后加参数不用改函数签名。
     *
     * @param  cfg  Hc595Config 结构体（包含 ds/shcp/stcp/num_chips）
     */
    int begin(const Hc595Config &cfg);

    /**
     * @brief  初始化（用 Hc595Config + LedMap，v2.0.0 新增，最推荐）
     *
     *   功能最强的初始化方式，一行配置完所有东西。
     *   初始化后所有 API 都能用：底层 + 效果函数。
     */
    int begin(const Hc595Config &cfg, const LedMap *leds, int led_count);

    // ========================================================================
    // ★ v2.0.0 新 API（数组方式，推荐用于芯片级操作）
    // ========================================================================

    /**
     * @brief  用数组直接写入所有芯片的输出状态
     *
     *   最底层、最灵活的 API。arr 数组每个元素对应一片 74HC595：
     *     arr[0] = U1 的 8bit 输出（对应 Q1~Q0）
     *     arr[1] = U2 的 8bit 输出
     *     ...
     *   bit 约定：bit0=Q1(引脚1), bit1=Q2(引脚2), ... bit7=Q0(引脚15)
     *
     *   内部会把 arr 内容 memcpy 到 current_output_，然后调 refresh_hw_() 发到硬件。
     *   如果 count < num_chips，多余的片会被强制清零（安全处理）。
     *   如果 count > num_chips，会被截断（安全处理）。
     *
     * @param  arr    数组指针：arr[0]=U1, arr[1]=U2, ...
     * @param  count  需要发送的片数（1 ~ num_chips）
     *
     * 用法示例：
     *   uint8_t chips[2] = {0};      // 2 片全灭
     *   chips[0] = 0b00000110;       // U1.Q1 和 U1.Q2 亮
     *   chips[1] = 0b00000001;       // U2.Q1 亮
     *   drv.write_array(chips, 2);
     */
    void write_array(const uint8_t *arr, int count);

    /**
     * @brief  单独点亮某片某 Q 脚（其他保持不变）
     *
     *   内部操作：current_output_[chip] |= (1 << q) → refresh_hw_()
     *   参数做了边界检查，chip 越界或 q 越界直接 return。
     *
     * @param  chip  片序号：0=U1, 1=U2, 2=U3 ...
     * @param  q     Q 脚编号：1=Q1, 2=Q2, ... 7=Q7, 0=Q0
     */
    void set_chip_q(int chip, int q);

    /**
     * @brief  单独灭掉某片某 Q 脚（其他保持不变）
     *
     *   内部操作：current_output_[chip] &= ~(1 << q) → refresh_hw_()
     */
    void clr_chip_q(int chip, int q);

    /**
     * @brief  翻转某片某 Q 脚（亮变灭，灭变亮）
     *
     *   内部操作：current_output_[chip] ^= (1 << q) → refresh_hw_()
     */
    void toggle_chip_q(int chip, int q);

    /**
     * @brief  读取某片当前的内部状态
     *
     *   返回 current_output_[chip]，不是直接从硬件读回（595 不能回读）。
     *
     * @param  chip  片序号：0=U1, 1=U2, ...
     * @return 该片当前 8bit 输出值；chip 越界返回 0
     */
    uint8_t get_chip(int chip) const;

    // ========================================================================
    // ★ v1.0.0 旧 API（完全保留，内部已改写为转调数组方式）
    // ========================================================================

    /**
     * @brief  用一个 uint32_t 写入完整状态（v1.0.0 兼容）
     *
     *   位分配：低 8bit=U1, 次 8bit=U2, 再高 8bit=U3, 再高 8bit=U4
     *   注意：**仅支持 ≤4 片**（32bit 限制）。更多片请用 write_array()。
     *
     * @param  data  32bit 状态值
     *
     * 用法示例：
     *   drv.write(0x0001);    // U1.Q1 亮
     *   drv.write(0x0102);    // U1.Q2 + U2.Q1 亮
     */
    void write(uint32_t data);

    /**
     * @brief  单独点亮某一位（全局位编号，v1.0.0 兼容）
     *
     *   bit 编号约定：
     *     bit0 = U1.Q1, bit1 = U1.Q2, ... bit7 = U1.Q0
     *     bit8 = U2.Q1, bit9 = U2.Q2, ... bit15 = U2.Q0
     *     bit16 = U3.Q1, ...
     *   内部自动换算成 (chip, q) 后调 set_chip_q()。
     */
    void set_bit(int bit);

    /**
     * @brief  单独灭掉某一位（全局位编号）
     *
     *   内部自动换算成 (chip, q) 后调 clr_chip_q()。
     */
    void clr_bit(int bit);

    /**
     * @brief  翻转某一位（全局位编号）
     */
    void toggle_bit(int bit);

    /**
     * @brief  所有片的所有 Q 脚全部输出低电平（LED 全灭）
     *
     *   内部：memset(current_output_, 0, num_chips_) → refresh_hw_()
     */
    void clear();

    /**
     * @brief  所有片的所有 Q 脚全部输出高电平（LED 全亮）
     *
     *   内部：memset(current_output_, 0xFF, num_chips_) → refresh_hw_()
     */
    void all_on();

    /**
     * @brief  读取当前输出状态（v1.0.0 兼容，仅返回前 32bit）
     *
     *   把 current_output_[0..3] 拼成一个 uint32_t 返回。
     *   超过 4 片的部分丢失（旧 API 限制）。
     */
    uint32_t get_output() const;

    // ========================================================================
    // 查询函数
    // ========================================================================

    /**
     * @brief  查询：begin() 是否成功执行
     */
    bool is_ready() const { return initialized_; }

    /**
     * @brief  查询：当前配置的级联片数
     */
    int chip_count() const { return num_chips_; }

    /**
     * @brief  查询：LED 映射表中的 LED 数量
     *
     *   如果没有用带 LedMap 的 begin()，返回 0。
     */
    int led_count() const { return led_count_; }

    /**
     * @brief  查询：是否已配置 LedMap（效果函数能用吗？）
     *
     *   true  = 已配置 LedMap，可以用 self_test() / water_flow() / led_on()
     *   false = 没配置，只能用底层 API
     */
    bool has_led_map() const { return leds_ != nullptr && led_count_ > 0; }

    // ========================================================================
    // 流水灯模式（v1.0.0 兼容，全局 bit 编号方式）
    // ========================================================================

    /**
     * @brief  启动流水灯模式（v1.0.0 兼容）
     *
     *   和 water_flow() 区别：
     *     flow() 用全局 bit 编号，不带 LedMap
     *     water_flow() 用 LedMap，更直观
     *
     * @param  start_bit  起始位（如 1 = 从 U1.Q1 开始）
     * @param  count      灯数量（如 6 = 6 个灯依次亮）
     * @param  delay_ms   每步间隔（毫秒）
     * @param  bounce     false=单向循环(0→1→2→...→0)，true=往返(0→1→2→1→0)
     */
    void flow(int start_bit, int count, uint32_t delay_ms, bool bounce = false);

    /**
     * @brief  停止流水灯模式
     *
     *   设置 flow_enabled_=false + clear()。
     */
    void flow_stop();

    /**
     * @brief  流水灯驱动（v1.0.0 的 flow 模式）
     *
     *   必须在 loop() 里调用。内部用 millis() 做非阻塞延时。
     *   注意：**有 delay() 的 self_test() / water_flow() 可以不用 loop()**，
     *   但 **flow() + loop()** 是非阻塞方式，推荐需要同时做其他事的场景。
     */
    void loop();

    // ========================================================================
    // ★ v2.0.0 新增：LedMap 效果函数（最上层，用 LedMap 控制）
    //
    //   只有用带 LedMap 的 begin() 初始化后才能用！
    //   否则 has_led_map() 返回 false，这些函数直接 return。
    // ========================================================================

    /**
     * @brief  按 LedMap 顺序逐个点亮再灭掉（自检）
     *
     *   顺序：k_leds[0] 亮 → 800ms → 灭 → k_leds[1] 亮 → 800ms → 灭 → ...
     *   内部用 delay() 阻塞，所以只适合 setup() 里跑一次。
     *   每步会打印 Serial 日志方便调试。
     */
    void self_test();

    /**
     * @brief  流水灯：按 LedMap 顺序循环点亮（阻塞方式）
     *
     *   每步：亮 → 500ms → 灭 → 200ms → 下一个
     *   因为内部用了 delay()，必须在 loop() 里调（loop 本身就是循环的）。
     *
     *   和 flow() + loop() 的区别：
     *     water_flow() 阻塞但简单，流水顺序 = LedMap 数组顺序
     *     flow() + loop() 非阻塞但复杂，流水顺序靠 start_bit/count
     */
    void water_flow();

    /**
     * @brief  用 LedMap 点亮指定索引的 LED
     *
     * @param  index  k_leds 数组下标：0 ~ led_count_-1
     *
     *   内部：set_chip_q(leds_[index].chip, leds_[index].q)
     */
    void led_on(int index);

    /**
     * @brief  用 LedMap 灭掉指定索引的 LED
     */
    void led_off(int index);

private:
    // ========================================================================
    // 私有成员变量
    // ========================================================================

    // --- 硬件引脚 ---
    int ds_pin_;       // 串行数据引脚（初始化时从 begin() 保存）
    int shcp_pin_;     // 移位时钟引脚
    int stcp_pin_;     // 锁存时钟引脚
    int num_chips_;    // 级联片数（begin() 时确定，之后不变）
    bool initialized_; // begin() 成功后置 true，所有函数先检查这个

    // --- ★ v2.0.0：内部状态数组（核心！所有输出都经过这里）---
    //
    //   current_output_[0] = U1 的 8bit（bit0=Q1, bit1=Q2, ... bit7=Q0）
    //   current_output_[1] = U2 的 8bit
    //   ...
    //   current_output_[num_chips_-1] = 最后一片的 8bit
    //
    //   修改流程：
    //     1. set_chip_q() / clr_chip_q() / write_array() —— 只改这个数组
    //     2. refresh_hw_() —— 把数组内容发到硬件
    //   好处：多次改数组可以最后统一锁存一次，避免输出中间态闪烁
    uint8_t current_output_[HC595_MAX_CHIPS];

    // --- ★ v2.0.0 新增：LED 映射表（可选，只有带 LedMap 的 begin() 才赋值）---
    const LedMap *leds_;      // 指针，指向外部的 k_leds 数组（外部必须保证生命周期）
    int           led_count_; // LED 数量
    int           led_flow_idx_;  // water_flow() 当前第几个 LED（0 ~ led_count_-1）

    // --- 流水灯状态机（v1.0.0 flow 模式用）---
    bool     flow_enabled_;   // flow() 启动后置 true，flow_stop() 置 false
    int      flow_start_;     // 起始 bit 编号
    int      flow_count_;     // 灯数量
    uint32_t flow_delay_ms_;  // 每步间隔（毫秒）
    bool     flow_bounce_;    // false=单向，true=往返
    int      flow_idx_;       // 当前第几步（0 ~ flow_count_-1）
    int      flow_dir_;       // 方向：1=正向，-1=反向（bounce 模式用）
    uint32_t flow_last_ms_;   // 上次更新的 millis() 值（非阻塞延时用）

    // ========================================================================
    // 私有底层函数（外部不能调）
    // ========================================================================

    /**
     * @brief  发一个 byte 到 74HC595 链
     *
     *   时序：8 次循环，每次 ds 置位 → shcp 上升沿 → shcp 下降沿
     *   数据高位先发（MSB first）：bit7 先发，bit0 最后发
     *   delayMicroseconds(1) 是为了让 74HC595 有足够的建立时间
     *
     * @param  data  要发送的 8bit 数据
     */
    void shift_out_byte_(uint8_t data);

    /**
     * @brief  STCP 锁存脉冲
     *
     *   所有 bit 发完后，STCP 上升沿把移位寄存器的数据锁存到输出寄存器。
     *   之前的 delayMicroseconds(1) 是数据建立时间要求。
     */
    void latch_();

    /**
     * @brief  ★ v2.0.0 核心：把 current_output_ 数组一次性发到硬件
     *
     *   调用链：set_chip_q() / write_array() / clear() → refresh_hw_()
     *
     *   发送顺序（关键！）：
     *     for (int i = num_chips_-1; i >= 0; i--)  // 从最后一片往前发
     *         shift_out_byte_(current_output_[i]);
     *
     *   为什么要从后往前发？
     *     74HC595 是串行移位寄存器，每来一个 SHCP 所有数据整体后移一位。
     *     先发的 byte 会被推到"更远"的芯片（QH' 方向）。
     *     最后发的 byte 留在"最近"的芯片（DS 输入那片）。
     *     所以：先发 U3（最远），再发 U2，最后发 U1（最近）。
     *
     *   发完所有 byte 后调 latch_() 统一锁存，所有芯片同时更新输出。
     */
    void refresh_hw_();
};

// ============================================================================
// 版本标记
// ============================================================================
//
//   如果需要在运行时检测库版本，可以用这个宏：
//   Serial.printf("Hc595Driver v%d.%d.%d\n",
//                 HC595_VERSION_MAJOR, HC595_VERSION_MINOR, HC595_VERSION_PATCH);
//
#define HC595_VERSION_MAJOR 2
#define HC595_VERSION_MINOR 0
#define HC595_VERSION_PATCH 0

// v2.0.0 用法示例见文件顶部注释
