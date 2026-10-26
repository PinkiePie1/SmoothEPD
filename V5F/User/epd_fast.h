#ifndef EPD_FAST_H
#define EPD_FAST_H

#include <stdint.h>

/* 1.54 inch 152x152 SSD1680, black and white only.
 *
 * Frame format: 19 bytes per row, 2888 bytes, MSB is the leftmost pixel,
 * bit 1 = WHITE and bit 0 = BLACK.
 *
 * Wiring (CH32V303VCT6), 全部 PA 口：
 *   SCK  PA5  SPI1
 *   MOSI PA7  SPI1
 *   CS   PA4  软件片选
 *   DC   PA3
 *   RST  PA2
 *   BUSY PA1  输入上拉
 *
 * 具体引脚宏在 epd_fast.c 顶部的「移植区」里，换板子改那里即可。
 */

#define EPD_W 152
#define EPD_H 152
#define EPD_STRIDE (EPD_W / 8)
#define EPD_FRAME_BYTES (EPD_STRIDE * EPD_H)

/* ------------------------------------------------------------- commands */

void EPD_Cmd(uint8_t cmd);
void EPD_Dat(uint8_t dat);


/* ------------------------------------------------------------------ 延时 */

void EPD_DelayMs(uint32_t ms);

void EPD_TickInit(void);
uint32_t EPD_TickNow(void);
/* 单调累计毫秒；每次调用处理 SysTick 回绕，不直接用 32 位计数换算总时长。 */
uint32_t EPD_Millis(void);

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

/* 目标变为黑/白时分别装入的扫描脉冲预算；V5F 侧仅保留代理接口。 */
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
 * advance one scan per call. EPD_FastFrame returns 1 if it actually drove.
 *
 * Handled inside EPD_FastFrame, no caller work needed:
 *   - HOLD ending: once the change has been driven out, one extra scan with all
 *     masks clear lets the waveform finish cleanly (see EPD_HOLD_ENDING).
 *   - V3F 每 1000 次快刷做小结账，每 4000 次直接全刷并清零账本。 */
void EPD_FastBegin(const uint8_t *shown);

/* 推进一帧。激活后**立即返回**，面板自己去刷那 20 ms，CPU 可以继续算下一帧；
 * 下一次调用会自动等上一帧刷完再激活。想主动等就用 EPD_FastSync()。 */
int EPD_FastFrame(const uint8_t *next);

/* 等面板把已激活的那一帧刷完（阻塞）。 */
void EPD_FastSync(void);

/* Fast scans since the last full refresh (the clean counter above). */
uint32_t EPD_FastScanCount(void);

/* ---------------------------------------------------------------- sleep */

void EPD_Sleep(void);

#endif
