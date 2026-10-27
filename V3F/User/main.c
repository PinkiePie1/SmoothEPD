/* CH32H417 双核 —— V3F（显示运算 + 驱动面板 + 系统串口）
 *
 *   V5F ：内容生成，写进共享内存的 A/B 缓冲
 *   V3F ：从共享内存取帧并显示。
 *         顺便是整个系统的串口（自己的 printf 直发，V5F 的走 IPC 转发）
 *
 *   两核读写的是同一块物理内存
 */

#include <string.h>

#include "debug.h"
#include "hardware.h"
#include "v5console.h"
#include "epd_share.h"
#include "print.h"
#include "epd_fast.h"

/* 多久没有新帧到来就认为demo结束 */
#define EPD_IDLE_MS     4000u   /* 多久没新帧算结束 */
#define EPD_BLANK_DELAY_MS  10000u  /* 最后一帧全刷后，等多久再刷成白屏 */


/*------------初始化时基等函数----------------*/
static void app_tick_init(void)
{
    SysTick0->CTLR = 0;
    SysTick0->CNT = 0;
    SysTick0->CMP = 0xFFFFFFFFu;
    SysTick0->CTLR = (1u << 2) | (1u << 0);
}

static uint32_t app_tick_now(void)
{
    return SysTick0->CNT;
}

static uint32_t ms_since(uint32_t t0)
{
    return (uint32_t)(app_tick_now() - t0) / (HCLKClock / 1000u);
}

static int epd_restart_fast(const uint8_t *image)
{
    EPD_SendDisplay(image);
    if (EPD_TakeTimeoutFlag()) {
        return -1;
    }
    EPD_FastBegin(image);
    EPD_FastPrepare();
    return EPD_TakeTimeoutFlag() ? -1 : 0;
}

/* 主函数。驱动用法看这里
单核基础流程很简单。首先调用EPD_Hal_Init()初始化单片机外设
然后调用epd_restart_fast(image)开始第一帧刷新。（image为你的显示数据数组）
随后每生成一帧就调用EPD_FastFrame(image)把新帧送进去即可。fastframe是非阻塞的。

当然epd_restart_fast函数是写在main里的，它实际做的事情是：
EPD_SendDisplay 重置并初始化墨水屏，并全刷第一帧
随后用EPD_FastBegin和EPD_FastPrepare 初始化快速刷新所用的数据。
上述两个调用完之后就可以进行快速刷新了。

在任意时刻都可以调用EPD_SendDisplay(image)直接全刷一次，该函数包含重启墨水屏并刷新等所有逻辑。
epd_restart_fast(image)

*/

int main(void)
{
    SystemInit();
    SystemAndCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);          /* USART1 / PA9，115200 */

    v5con_init();                       // V5核的printf转发初始化。两边的打印都从v3f的串口输出

    printf("\r\n[V3F] boot, SystemClk=%u, HCLK=%u\r\n",
           (unsigned)SystemClock, (unsigned)HCLKClock);

    PWR->CTLR |= PWR_CTLR_VSEL_VIO18_MODE7|PWR_CTLR_VIO_SWCR;// turn off VIO18 power.

    epd_share_init();
    app_tick_init();
    EPD_Hal_Init();
    EPD_DelayMs(20);

    NVIC_WakeUp_V5F(Core_V5F_StartAddr);
    printf("[V3F] V5F released, waiting for frames...\r\n");

    {
        static uint8_t shown[EPD_FRAME_BYTES];
        uint32_t started = 0;
        uint32_t t_report = 0;
        uint32_t t_last = 0;            /* 最近一次收到新帧的时刻 */
        uint32_t n_report = 0;

        for (;;) {
            v5con_poll();               /* 转发 V5F 的日志*/

            const uint8_t *next = epd_share_peek();//看看V5有没有发新帧过来

            if (next == 0) {
                
                //如果没有发新帧，看看多久没发新帧了，如果超过两秒就停手清屏。
                //如果希望它持续显示的话，V5应该一直发同一帧。
                if (started && (ms_since(t_last) >= EPD_IDLE_MS)) {
                    printf("[V3F] producer idle %u ms, finishing\r\n",
                           (unsigned)ms_since(t_last));

                    /* shown 是上一帧的本地快照；共享槽可能已经被 V5F 复用 */
                    EPD_SendDisplay(shown);     // 全刷
                    if (EPD_TakeTimeoutFlag()) {
                        printf("[V3F] final full refresh failed\r\n");
                        goto finish_halt;
                    }
                    printf("[V3F] full refresh done\r\n");

                    //等10秒再刷白屏
                    {
                        const uint32_t t_blank = app_tick_now();

                        while (ms_since(t_blank) < EPD_BLANK_DELAY_MS) {
                            v5con_poll(); //转发V5日志。
                            EPD_SleepUs(2000);   /* 等 10 秒期间也让 CPU 睡 */
                        }
                    }

                    // 刷白并关闭。
                    memset(shown, 0xFF, EPD_FRAME_BYTES);
                    EPD_SendDisplay(shown);
                    if (EPD_TakeTimeoutFlag()) {
                        printf("[V3F] blank refresh failed\r\n");
                        goto finish_halt;
                    }
                    printf("[V3F] blank screen done\r\n");

                    EPD_Sleep();
                    if (EPD_TakeTimeoutFlag()) {
                        printf("[V3F] EPD sleep failed\r\n");
                        goto finish_halt;
                    }
                    printf("[V3F] EPD off\r\n");

                  //然后V3就不干活了，一直转发串口
                  //demo是这样的。正常使用的话当然还是需要其他逻辑
finish_halt:
                    for (;;) {
                        v5con_poll();
                        EPD_SleepUs(10000);
                    }
                }
                /* 没有新帧：WFI 等 2 ms；新帧的 IPC 中断会立刻把 CPU 叫醒 */
                EPD_SleepUs(2000);
                continue;  //没有新帧，但也没到等待时间，回去继续等。
            }

            t_last = app_tick_now();

            // 先取到本地快照，再立刻把共享槽还给 V5F 并唤醒它。
            // 后面的等 BUSY / 送 SPI / 全刷都只读本地的 shown，
            // V5F 可以同时往另一块缓冲里算下一帧。
            memcpy(shown, next, EPD_FRAME_BYTES);
            epd_share_release();

            //如果是第一帧的话，先全刷一下再进快速模式。
            if (!started) {
                if (epd_restart_fast(shown) != 0) {
                    printf("[V3F] EPD init timeout, retrying\r\n");
                    continue;
                }

                started = 1;
                printf("[V3F] first frame in, fast mode ready\r\n");
                t_report = app_tick_now();
                n_report = 0;
                continue;
            }

            // 正常帧：整条显示流水线都在这里
            // 显示就是不断调用这个函数，往里面塞新帧就行了
            (void)EPD_FastFrame(shown);
            //超时重启
            if (EPD_TakeTimeoutFlag()) {
                printf("[V3F] EPD timeout, recovering\r\n");
                if (epd_restart_fast(shown) != 0) {
                    printf("[V3F] EPD recovery failed\r\n");
                }
            }
            ++n_report;

            //这块是FPS统计。
            {
                const uint32_t dt = ms_since(t_report);
                if (dt >= 1000u) {
                    t_report = app_tick_now();

                    EPD_SHARE->cons_fps = n_report * 10000u / dt;  
                    PRINT("[V3F] fps=%u | drop=%u\r\n",
                          (unsigned)(n_report * 1000u / dt),
                          (unsigned)epd_share_dropped());
                    n_report = 0;
                }
            }
        }
    }
}
