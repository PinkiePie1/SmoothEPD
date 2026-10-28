/* ============================================================================
 * V5F 侧的驱动
 *
 * demo（main.c）原本直接调 EPD_* 驱动。现在改成V3F核显示之后这里大部分变成套壳了，不用改demo代码。
 *
 *   EPD_FastFrame(frame)  ->  把 frame 拷进共享 A/B 缓冲并提交（write_seq++）
 *   其它 EPD_Init / EPD_SendDisplay / EPD_FastBegin / EPD_FastPrepare /
 *   EPD_Sleep 全变成空操作（面板动作都由 V3F 做）
 *
 * 
 * ==========================================================================*/

#include "epd_share.h"
#include "epd_fast.h"

#include <string.h>

/* ----------------------------------------------------------------- 时基
 * 只用来给 demo 算 elapsed_ms / fps。RTC 由 V3F 初始化成 1 ms 一格，
 * 在 Stop 下继续走，睡眠期间的时间不会丢。 */
static uint32_t s_tick_base;

void EPD_TickInit(void)
{
    s_tick_base = RTC_GetCounter();
}

uint32_t EPD_TickNow(void)
{
    return RTC_GetCounter();
}

uint32_t EPD_Millis(void)
{
    return (uint32_t)(EPD_TickNow() - s_tick_base);
}

/* 粗略的毫秒延时：只为驱动初始化和收尾用，精度不敏感 */
void EPD_DelayMs(uint32_t ms)
{
    while (ms-- != 0u) {
        Delay_Ms(1);
    }
}

/* ----------------------------------------------------------------- 生产者 */
static uint32_t s_scan_count;

void EPD_Hal_Init(void)
{
    epd_share_init();
}

void EPD_Init(void)
{
}

void EPD_SendDisplay(const uint8_t *image)
{
    (void)image;
}

void EPD_FastBegin(const uint8_t *shown)
{
    (void)shown;
}

void EPD_FastPrepare(void)
{
}

void EPD_SetPulseBudget(uint8_t black, uint8_t white)
{
    (void)black;
    (void)white;
}

//发送显示
//注意帧不要太快，显示的内容要尽量保持连续性
//否则很容易电荷不平衡，尤其是高速运动的细线
//对于此类内容，需要降低运动速度，减少全刷间隔等措施

int EPD_FastFrame(const uint8_t *image)
{
    uint8_t *slot = epd_share_begin();

    if (slot != 0) {
        memcpy(slot, image, EPD_FRAME_BYTES);
        epd_share_commit();
        ++s_scan_count;
    }

    return 0;
}

uint32_t EPD_FastScanCount(void)
{
    return s_scan_count;
}

void EPD_Sleep(void)
{
}
