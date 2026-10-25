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
 * 只用来给 demo 算 elapsed_ms / fps。SysTick0 数的是 HCLK。 */
static uint32_t s_elapsed_ms;
static uint32_t s_last_cnt;
static uint32_t s_frac;

void EPD_TickInit(void)
{
    SysTick0->CTLR = 0;
    SysTick0->CNT = 0;
    SysTick0->CMP = 0xFFFFFFFFu;
    SysTick0->CTLR = (1u << 2) | (1u << 0);   /* HCLK 源 + 使能自由运行 */
    s_last_cnt = SysTick0->CNT;
    s_elapsed_ms = 0;
    s_frac = 0;
}

uint32_t EPD_TickNow(void)
{
    return SysTick0->CNT;
}

uint32_t EPD_Millis(void)
{
    const uint32_t now = EPD_TickNow();
    const uint32_t delta = now - s_last_cnt;
    const uint64_t accumulated = (uint64_t)delta * 1000u + s_frac;

    s_last_cnt = now;
    s_elapsed_ms += (uint32_t)(accumulated / HCLKClock);
    s_frac = (uint32_t)(accumulated % HCLKClock);
    return s_elapsed_ms;
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

uint8_t first = 0;
int EPD_FastFrame(const uint8_t *image)
{
    uint8_t *slot = epd_share_begin();

    if (slot != 0) {
        memcpy(slot, image, EPD_FRAME_BYTES);
        epd_share_commit();
        ++s_scan_count;
    }

    if(first == 0){
        first = 1;
        Delay_Ms(2800); //第一帧等一下，让那边全刷完了再开始
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
