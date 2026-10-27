#ifndef EPD_FAST_H
#define EPD_FAST_H

#include <stdint.h>

/* 1.54 inch 152x152 SSD1680, black and white only.
 *
 * Frame format: 19 bytes per row, 2888 bytes, MSB is the leftmost pixel,
 * bit 1 = WHITE and bit 0 = BLACK.
 *
 *   默认使用SPI1。在V303和H417上都是
 *   SCK  PA5  SPI1
 *   MOSI PA7  SPI1
 *   其他接线参考.c文件
 *
 * 具体引脚宏在 epd_fast_net.c 顶部的「移植区」里，换板子改那里即可。
 */
#define EPD_W 152
#define EPD_H 152
#define EPD_STRIDE (EPD_W / 8)
#define EPD_FRAME_BYTES (EPD_STRIDE * EPD_H)

/* ------------------------------------------------------------- commands */

void EPD_Cmd(uint8_t cmd);
void EPD_Dat(uint8_t dat);


/* ------------------------------------------------------------------ 延时 */

/* 纯软件延时。 */
void EPD_DelayUs(uint32_t us);
void EPD_DelayMs(uint32_t ms);

/* 低功耗等待：WFI，SysTick0 定时唤醒；任意中断也会提前唤醒。 */
void EPD_SleepUs(uint32_t us);

/* ---------------------------------------------------------------- setup */

void EPD_Hal_Init(void);
void EPD_Init(void);

/* --------------------------------------------------------- full refresh */

/* Full refresh, measured 1314 ms. */
void EPD_SendDisplay(const uint8_t *image);

/* ---------------------------------------------------------- fast refresh */

/* Prepare the fast waveform. Call once after a full refresh, then advance one
 * scan per EPD_FastFrame. One controller frame per scan is the fastest
 * (26 ms); 8 frames is much stronger but 166 ms.
 *
 * The waveform may not be reloaded while the booster is up, so the frame count
 * is fixed here, in one shot. */
void EPD_FastPrepare(void);
void EPD_FastPrepareFrames(uint8_t frames);

/* 目标变为黑/白时分别装入的扫描脉冲预算；0 按 1 处理。 */
void EPD_SetPulseBudget(uint8_t black, uint8_t white);

/* 帧率限制：等 BUSY 落下后额外等待 us 微秒再写下一帧；us = 0 表示不限速。
 * EPD_SetTargetFps() 会按 (1e6/fps - 固定开销) 自动算 hold。 */
void EPD_SetFrameHoldUs(uint32_t us);
void EPD_SetTargetFps(uint16_t fps);

/* One fast scan: drive the pixels in black toward black and the pixels in
 * white toward white. Pixels in neither mask keep their current colour, so the
 * two masks must not overlap. */
void EPD_FastScan(const uint8_t *black, const uint8_t *white);

/* Remember what the panel is showing (call right after a full refresh), then
 * advance one scan per call. EPD_FastFrame returns 1 if it actually drove,
 * 0 for a HOLD-only call, or -1 when a panel timeout prevented progress.
 *
 * Handled inside EPD_FastFrame, no caller work needed:
 *   - HOLD ending: once the change has been driven out, one extra scan with all
 *     masks clear lets the waveform finish cleanly (see EPD_HOLD_ENDING).
 *   - soft settlement: every EPD_SETTLE_INTERVAL scans without a full refresh.
 *     EPD_SOFT_SETTLE_ALL=0 limits repayment to the target direction; =1 repays
 *     every nonzero debt once, including the opposite direction.
 *   - periodic clean: every EPD_CLEAN_INTERVAL scans, runs a full refresh
 *     directly and clears the debt after success. Defaults: soft every 1000,
 *     full every 4000 scans. */
void EPD_FastBegin(const uint8_t *shown);

/* 推进一帧。激活后直接返回。面板刷新一次大概20ms，这段时间CPU 可以继续算下一帧；
 * 下一次调用会自动等上一帧刷完再激活。想主动等就用 EPD_FastSync()。 */
int EPD_FastFrame(const uint8_t *next);

/* 等面板把已激活的那一帧刷完（阻塞）。 */
void EPD_FastSync(void);

/* Fast scans since the last full refresh (the clean counter above). */
uint32_t EPD_FastScanCount(void);

/* 上一次 FastFrame 是否遇到 BUSY 超时（读后清零）；1 = 面板可能卡住，建议重初始化 */
uint8_t EPD_TakeTimeoutFlag(void);

/* ---------------------------------------------------------------- sleep */

void EPD_Sleep(void);

#endif
