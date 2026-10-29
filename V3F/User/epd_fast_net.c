/* ============================================================================
 * SSD1680 152x152 电子纸高刷驱动 —— CH32H417（跑在 V3F 上）
 *
 * 换单片机时只需要改两处：
 *   1. 下面的 #include（board.h 里选 SDK 头文件）
 *   2. 紧跟其后的「移植区」：引脚宏、SPI/DMA 参数、时基、收发原语、EPD_Hal_Init()
 * 移植区之后是纯驱动逻辑，和具体单片机无关。
 * ==========================================================================*/

#include <string.h>

#include "ch32h417.h"
#include "print.h"
#include "epd_fast.h"
#include "system_ch32h417.h"

/* ============================================================================
 * ============================ 移植区（开始） ================================
 *
 * 换一颗单片机要重写的就是这一段：
 *   - 引脚宏 CS/DC/RST/BUSY
 *   - SPI 时钟分频、DMA 通道与传输完成标志
 *   - 纯软件延时：EPD_DelayUs / EPD_DelayMs
 *   - 收发原语：spi_byte / EPD_Cmd / EPD_Dat / epd_dma_send / epd_dat_buf
 *   - EPD_Hal_Init()：时钟、GPIO、SPI、DMA 的初始化
 * ==========================================================================*/

/* ---------------------------------------------------------------- 引脚
 *
 * 全部选在右排连续的 A3 A4 A5 A6 A7 C4 上，接线最省事：
 *
 *   PA3  DC        PA4  CS         PA5  SPI1 SCK（AF5）
 *   PA6  RST       PA7  SPI1 MOSI（AF5）
 *   PC4  BUSY（输入下拉，EXTI4 用来唤醒 WFI）
 */
#define EPD_CS_PORT     GPIOA
#define EPD_CS_PIN      GPIO_Pin_4
#define EPD_DC_PORT     GPIOA
#define EPD_DC_PIN      GPIO_Pin_3
#define EPD_RST_PORT    GPIOA
#define EPD_RST_PIN     GPIO_Pin_6
#define EPD_BUSY_PORT   GPIOC
#define EPD_BUSY_PIN    GPIO_Pin_4

/* BUSY 引脚接到哪根 EXTI / 哪个中断号（WFI 靠它唤醒） */
#define EPD_BUSY_EXTI_LINE  EXTI_Line4
#define EPD_BUSY_IRQn       EXTI7_0_IRQn    // H417 的 EXTI 是分组中断
#define EPD_BUSY_EXTI_PORT  GPIO_PortSourceGPIOC
#define EPD_BUSY_EXTI_PIN   GPIO_PinSource4

#define CS_H()   GPIO_SetBits(EPD_CS_PORT, EPD_CS_PIN)
#define CS_L()   GPIO_ResetBits(EPD_CS_PORT, EPD_CS_PIN)
#define DC_H()   GPIO_SetBits(EPD_DC_PORT, EPD_DC_PIN)
#define DC_L()   GPIO_ResetBits(EPD_DC_PORT, EPD_DC_PIN)
#define RST_H()  GPIO_SetBits(EPD_RST_PORT, EPD_RST_PIN)
#define RST_L()  GPIO_ResetBits(EPD_RST_PORT, EPD_RST_PIN)
#define BUSY()   ((GPIO_ReadInputDataBit(EPD_BUSY_PORT, EPD_BUSY_PIN) != 0) ? 1u : 0u)

/* --------------------------------------------------------- SPI / DMA 参数 */

/* SPI1 位于 100 MHz HB2 总线。SSD1681 规定 SPI 写时钟最高 20 MHz，
 *   但这个芯片实际速率要更高一些，这里试过就算是/2也能用
 *   Mode0=/2  Mode1=/4  Mode2=/8  Mode3=/16 ... */
#define EPD_SPI_PRESCALER   SPI_BaudRatePrescaler_Mode0
#define EPD_GPIO_SPEED      GPIO_Speed_Very_High

// DMA设置。bulk传输走dma最快，比轮询fifo更快。
// 移植到其他平台的话这些宏可以删掉，重新实现epd_dma_send即可。
// 不想实现直接在该函数里循环EPD_Dat()也行。
#define EPD_DMA_CHANNEL     DMA1_Channel3                     // SPI1_TX 固定走 DMA1 通道3
#define EPD_DMA_MUX         DMA_MuxChannel3                   // H417 要经 DMAMUX 连到 SPI1_TX
#define EPD_DMA_REQUEST     63u                               // SPI1_TX 的 DMA 请求号
#define EPD_DMA_TC_FLAG     DMA1_FLAG_TC3
#define EPD_DMA_TC_READY()  (DMA_GetFlagStatus(DMA1, EPD_DMA_TC_FLAG) != RESET)
#define EPD_DMA_TC_CLEAR()  DMA_ClearFlag(DMA1, EPD_DMA_TC_FLAG)
#define EPD_DMA_MIN_LEN     256                               // 超过这个长度才走 DMA

static volatile uint8_t s_dma_done;
static void epd_rtc_alarm(uint32_t ms);
static void epd_rtc_alarm_off(void);

/* 帧率限制：屏幕单帧刷新完成后等这么久才写下一帧。0 = 不限速。 */
#define EPD_FRAME_HOLD_US   0u
#define EPD_FRAME_FIXED_US  21260u

/* 软件延时 */
#define EPD_DELAY_LOOPS_PER_MS  18000u
#define EPD_DELAY_LOOPS_PER_US  (EPD_DELAY_LOOPS_PER_MS / 1000u)

void EPD_DelayUs(uint32_t us)
{
    uint32_t loops = us * EPD_DELAY_LOOPS_PER_US;

    while (loops != 0u) {
        --loops;
        __asm__ volatile ("nop");
    }
}

void EPD_DelayMs(uint32_t ms)
{
    uint32_t loops = ms * EPD_DELAY_LOOPS_PER_MS;

    while (loops != 0u) {
        --loops;
        __asm__ volatile ("nop");
    }
}

/* ------------------------------------------------------- 故障定位插桩
 *
 * 把每一处可能"无超时死等"的地方都加上边界：等不到就置故障位、记录所在阶段并跳出，
 * 让系统活着把元凶报出来，而不是整机僵死。
 */
#define EPD_FAULT_SPI_TXE     0x0001u
#define EPD_FAULT_SPI_RXNE    0x0002u
#define EPD_FAULT_SPI_BSY     0x0004u
#define EPD_FAULT_RTC_RTOFF   0x0008u
#define EPD_FAULT_BUSY        0x0010u
#define EPD_FAULT_CLK_RESTORE 0x0020u
#define EPD_FAULT_UART_TC     0x0040u
#define EPD_FAULT_CLK_HSE     0x0080u
#define EPD_FAULT_CLK_PLL     0x0100u
#define EPD_FAULT_CLK_SW      0x0200u
#define EPD_WAIT_GUARD        2000000u

volatile uint32_t g_epd_fault;
volatile uint32_t g_epd_fault_phase;
volatile uint32_t g_epd_phase;

void EPD_SetFault(uint32_t bits)
{
    const uint32_t before = g_epd_fault;

    g_epd_fault |= bits;
    if ((before & bits) != bits) {
        g_epd_fault_phase = g_epd_phase;
        PRINT("[EPD] STALL f=%04x ph=%u\r\n", (unsigned)bits, (unsigned)g_epd_phase);
    }
}

uint32_t EPD_TakeFaultFlags(void)
{
    const uint32_t f = g_epd_fault;

    g_epd_fault = 0u;
    return f;
}

uint32_t EPD_FaultPhase(void)
{
    return g_epd_fault_phase;
}

/* RTC 写寄存器要等 RTOFF；RTC 一旦没有时钟这里会永远等不到，必须加边界 */
static void epd_rtc_wait_ready(void)
{
    uint32_t guard = 0;

    while ((RTC->CTLRL & RTC_FLAG_RTOFF) == 0u) {
        if (++guard >= EPD_WAIT_GUARD) {
            EPD_SetFault(EPD_FAULT_RTC_RTOFF);
            return;
        }
    }
}

#define RTC_WaitForLastTask()  epd_rtc_wait_ready()

/* Stop 唤醒后的时钟恢复：必须在唤醒中断里立刻做（官方 Stop_Mode 例程即如此），
 * 否则会有一段代码跑在 HSI 上，而且恢复时机也不可控。 */
/* 官方 PWR_STOPMode_LP_Cmd(DISABLE) 的等价实现：把 SYS_CFGR0 写回进入 Stop 前的原值。
 * 原值在第一次调用时保存，之后每次唤醒都写回，避免 VDDK 档位被残留改变。 */
static uint32_t s_vddk_orig;
static uint8_t  s_vddk_ready;

static void epd_vddk_restore(void)
{
    if (s_vddk_ready == 0u) {
        s_vddk_orig = *(volatile uint32_t *)SYS_CFGR0_BASE;
        s_vddk_ready = 1u;
    }
    *(volatile uint32_t *)SYS_CFGR0_BASE = s_vddk_orig;
}

static void epd_clock_restore(void);



/* 单字节发送 */
static void spi_byte(uint8_t v)
{
    uint32_t guard = 0;

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
        if (++guard > 200000u) {
            return;
        }
    }
    SPI_I2S_SendData(SPI1, v);

    guard = 0;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {
        if (++guard > 200000u) {
            return;
        }
    }
    (void)SPI_I2S_ReceiveData(SPI1);
}

/* 命令 */
void EPD_Cmd(uint8_t cmd)
{
    DC_L();
    CS_L();
    spi_byte(cmd);
    CS_H();
}

/* 数据 */
void EPD_Dat(uint8_t dat)
{
    DC_H();
    CS_L();
    spi_byte(dat);
    CS_H();
}

/* 批量发送：DMA 把 buf 一次性推给 SPI1，传输期间 CPU 阻塞。
 * DMA 计数器 16 位，单次最多 65535 字节 */
static void epd_dma_send(const uint8_t *buf, uint32_t len)
{
    const uint32_t deadline = EPD_TickMs() + 5u;

    s_dma_done = 0;
    DMA_Cmd(EPD_DMA_CHANNEL, DISABLE);
    EPD_DMA_TC_CLEAR();
    EPD_DMA_CHANNEL->MADDR = (uint32_t)buf;
    DMA_SetCurrDataCounter(EPD_DMA_CHANNEL, (uint16_t)len);
    epd_rtc_alarm(5u);
    DMA_Cmd(EPD_DMA_CHANNEL, ENABLE);

    while ((s_dma_done == 0u) && ((int32_t)(deadline - EPD_TickMs()) > 0)) {
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
        __WFI();
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
    }
    epd_rtc_alarm_off();

    if ((s_dma_done == 0u) && !EPD_DMA_TC_READY()) {
        /* DMA 没跑起来就退回轮询，把没发完的部分补上 */
        const uint32_t left = DMA_GetCurrDataCounter(EPD_DMA_CHANNEL);
        const uint32_t done = (left <= len) ? (len - left) : len;

        DMA_Cmd(EPD_DMA_CHANNEL, DISABLE);
        for (uint32_t i = done; i < len; ++i) {
            spi_byte(buf[i]);
        }
        PRINT("[EPD] dma timeout at %u/%u, fell back to polling\r\n",
              (unsigned)done, (unsigned)len);
        return;
    }

    /* DMA 完成时最后一个字节可能还在移位寄存器里 */
    {
        uint32_t guard = 0;

        g_epd_phase = 2u;
        while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
            if (++guard >= EPD_WAIT_GUARD) {
                EPD_SetFault(EPD_FAULT_SPI_TXE);
                break;
            }
        }
        guard = 0;
        g_epd_phase = 3u;
        while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) {
            if (++guard >= EPD_WAIT_GUARD) {
                EPD_SetFault(EPD_FAULT_SPI_BSY);
                break;
            }
        }
        g_epd_phase = 1u;
    }

    DMA_Cmd(EPD_DMA_CHANNEL, DISABLE);
    EPD_DMA_TC_CLEAR();

    /* SPI传完要读一下接收 */
    (void)SPI1->DATAR;
    (void)SPI1->STATR;
}

void DMA1_Channel3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel3_IRQHandler(void)
{
    if (EPD_DMA_TC_READY()) {
        s_dma_done = 1;
        EPD_DMA_TC_CLEAR();
    }
}

/* 数据阶段（CS 保持低电平）：长块走 DMA，短块逐字节 */
static void epd_dat_buf(const uint8_t *buf, uint32_t len)
{
    DC_H();
    CS_L();
    if (len >= EPD_DMA_MIN_LEN) {
        epd_dma_send(buf, len);
    } else {
        for (uint32_t i = 0; i < len; ++i) {
            spi_byte(buf[i]);
        }
    }
    CS_H();
}

/* ------------------------------------------------------------ 硬件初始化 */

static void epd_rtc_ensure(void);

static void epd_hw_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    SPI_InitTypeDef spi = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOC | RCC_HB2Periph_GPIOE, ENABLE);

    /* CS / DC / RST 输出 */
    gpio.GPIO_Pin = EPD_CS_PIN | EPD_DC_PIN | EPD_RST_PIN;
    gpio.GPIO_Speed = EPD_GPIO_SPEED;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(EPD_CS_PORT, &gpio);

    /* 示波器观察用：唤醒高、Stop/WFI 睡眠低（PB1） */
    gpio.GPIO_Pin = GPIO_Pin_1;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &gpio);
    GPIO_SetBits(GPIOB, GPIO_Pin_1);

    /* BUSY 输入下拉 */
    gpio.GPIO_Pin = EPD_BUSY_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(EPD_BUSY_PORT, &gpio);

    /* BUSY中断配置*/
    GPIO_EXTILineConfig(EPD_BUSY_EXTI_PORT, EPD_BUSY_EXTI_PIN);
    {
        EXTI_InitTypeDef exti = {0};

        exti.EXTI_Line = EPD_BUSY_EXTI_LINE;
        exti.EXTI_Mode = EXTI_Mode_Interrupt;
        exti.EXTI_Trigger = EXTI_Trigger_Falling;
        exti.EXTI_LineCmd = ENABLE;
        EXTI_Init(&exti);
    }
    NVIC_SetPriority(EPD_BUSY_IRQn, 1);
    NVIC_EnableIRQ(EPD_BUSY_IRQn);

    /* SPI1 的 SCK/MOSI 在 PA5/PA7，AF5 */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF5);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF5);

    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    CS_H();
    DC_H();
    RST_H();

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_SPI1, ENABLE);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = EPD_SPI_PRESCALER;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);

    /* DMA：H417 要先经 DMAMUX 把通道3 连到 SPI1_TX */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_MuxChannelConfig(EPD_DMA_MUX, EPD_DMA_REQUEST);
    {
        DMA_InitTypeDef dma = {0};

        DMA_DeInit(EPD_DMA_CHANNEL);
        dma.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DATAR;
        dma.DMA_Memory0BaseAddr = 0;
        dma.DMA_DIR = DMA_DIR_PeripheralDST;
        dma.DMA_BufferSize = 0;
        dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
        dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
        dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
        dma.DMA_Mode = DMA_Mode_Normal;
        dma.DMA_Priority = DMA_Priority_VeryHigh;
        dma.DMA_M2M = DMA_M2M_Disable;
        dma.DMA_BufferMode = DMA_SingleBufferMode;
        DMA_Init(EPD_DMA_CHANNEL, &dma);
    }
    DMA_ITConfig(EPD_DMA_CHANNEL, DMA_IT_TC, ENABLE);
    NVIC_SetPriority(DMA1_Channel3_IRQn, 1);
    NVIC_EnableIRQ(DMA1_Channel3_IRQn);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
}

void EPD_Hal_Init(void)
{
    epd_hw_init();
    epd_rtc_ensure();
}

/* BUSY 引脚的 EXTI 中断：Stop 就靠它唤醒。
 * 唤醒后先恢复 VDDK + 时钟（官方例程在中断里做），再清标志。 */
void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void)
{
    epd_clock_restore();

    if (EXTI_GetITStatus(EPD_BUSY_EXTI_LINE) != RESET) {
        EXTI_ClearITPendingBit(EPD_BUSY_EXTI_LINE);
    }
}

/* ============================================================================
 * ============================= 移植区（结束） ===============================
 * ==========================================================================*/


/* ============================================================================
 * 墨水屏驱动参数：
 * ==========================================================================*/

/* 局部窗口开关：
 *   1 = 只把有变化的行推给面板（省传输，默认）
 *   0 = 每次都写整屏 152 行 */
#define EPD_PARTIAL_WINDOW  0

/* 每像素净驱动帧数限制：黑方向不超过 +7，白方向不超过 -7。 */
#define EPD_DC_BLACK        7
#define EPD_DC_WHITE        7
#define EPD_FAST_FRAMES     1      // 每次快刷扫描用几个控制器帧
#define EPD_RESET_HIGH_MS   50     // 保持 RST 高电平的时间 (ms)
#define EPD_RESET_LOW_MS    20     // 拉低 RST 的时间 (ms)
#define EPD_CLEAN_INTERVAL  2000   // 每多少次快刷做一次全刷清洁；0 = 关闭
#define EPD_HOLD_ENDING     1      // 1 = 最后一帧驱动完后补一次全0电平扫描放电。

#define EPD_PIXELS          (EPD_W * EPD_H)   /* 23104 */

/* ------------------------------------------------------------ waveforms */

/* Full refresh waveform taken from the user's CH572 driver, with the source
 * voltage fields flipped back to the polarity this driver uses. The flip is a
 * swap of the two bit fields 01 (VSH1) and 10 (VSL) in every VS field of the
 * four level rows, see Table 6-6 of the SSD1680 datasheet:
 *
 *   b = ((b & 0x55) << 1) | ((b & 0xAA) >> 1)
 *
 * 00 (VSS, no drive) and 11 (VSH2) are left alone, so untouched phases stay
 * untouched. Its stronger VSH1/VCOM (0x4A / 0x36) is what makes it fast.
 * Layout: 153 byte LUT, then 0x3F/0x03/0x04/0x2C take bytes 153..158. */
 /*
static const uint8_t lut_full[159] = {
    0x80,0x99,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,
    0x40,0x99,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,
    0x90,0x99,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,
    0x40,0x99,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0b,0x01,0x01,0x00,0x00,0x00,0x00,0x05,0x05,0x00,0x03,0x03,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x01,0x01,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x33,0x44,0x44,0x45,0x43,0x44,0x00,0x00,0x00,0x22,0x17,0x4a,
    0x00,0x38,0x36,
};
*/
static const uint8_t lut_full[159] =
{
0x80,	0x48,	0x40,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x40,	0x48,	0x80,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x80,	0x48,	0x40,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x40,	0x48,	0x80,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x9,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x5,	0x1,	0x0,	0x5,	0x1,	0x0,	0x3,					
0x9,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x22,	0x22,	0x22,	0x22,	0x22,	0x22,	0x0,	0x0,	0x0,			
0x22,	0x17,	0x41,	0x0,	0x32,	0x20                          //EOPT VGH VSH1 VSH2 VSL VCOM
};

/* Fast waveform: the vendor partial table with the two HOLD rows silenced.
 * Level rows are  00 = hold, 01 = drive white, 10 = drive black, 11 = hold,
 * so a pixel that is in neither mask keeps its current colour. */
static const uint8_t lut_fast_base[159] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 00 hold  */
    0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 01 white */
    0x40,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 10 black */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  /* 11 hold  */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,  /* 1 frame  */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x22,0x22,0x22,0x22,0x22,0x22,0x00,0x00,0x00,0x02,0x17,0x41,
    0xb0,0x32,0x28,
};

/* --------------------------------------------------------------- state */


static uint8_t analog_on;          /* the booster is up and kept up */
static uint8_t s_dc_black = EPD_DC_BLACK;
static uint8_t s_dc_white = EPD_DC_WHITE;
static uint8_t s_fast_frames = EPD_FAST_FRAMES;

/* 黑方向脉冲数减白方向脉冲数，限制在 [-7,+7]。 */
static int8_t s_net[EPD_PIXELS];
static int8_t s_net_next[EPD_PIXELS];
static uint8_t s_mask_black[EPD_FRAME_BYTES];
static uint8_t s_mask_white[EPD_FRAME_BYTES];
static uint8_t s_target[EPD_FRAME_BYTES];
static uint8_t s_active[EPD_FRAME_BYTES];
static uint8_t s_active_next[EPD_FRAME_BYTES];
static uint16_t s_planned_pixels[EPD_FRAME_BYTES];
static uint16_t s_planned_count;

static uint32_t s_scan_count;   /* fast scans since the last full refresh */
static uint8_t  s_pending;      /* 已经激活、面板还在刷 */
static uint8_t  s_row0, s_row1; /* 本帧要发的行范围（局部刷新用） */
static uint8_t  s_col0, s_col1; /* 本帧要发的列范围（字节列 0..18） */
static uint8_t s_hold_pending;  /* a scan drove something and still needs its HOLD ending */

/* ------------------------------------------------------------ 等待 BUSY
 *
 * Stop 模式：BUSY 高电平期间 CPU 进 Stop，BUSY 下降沿（一次波形结束）通过 EXTI 唤醒。
 * 一帧里大约 20 ms 都在这里睡，是最省电的一段。
 *
 * Stop 以后 HCLK 停摆，SysTick 不再计数，所以睡眠定时和 BUSY 超时都改用 RTC：
 *   - RTC 走 LSI，Stop 期间继续计数
 *   - 预分频后计数单位 1 ms，既当睡眠闹钟，也当 BUSY 超时
 *   - 每次进 Stop 前都装一个 RTC 闹钟，BUSY 一直不落也能按时醒来查超时
 */
#define EPD_RTC_PRESCALER_LSI   39u        /* LSI 40 kHz / 40 = 1 kHz，1 tick ≈ 1 ms */
#define EPD_LSI_WAIT_LOOPS      20000000u  /* LSI 起振等待上限，超时也继续往下走 */

/* BUSY 超时（单位：微秒）。正常快刷扫描 20~26 ms，全刷约 1.3 s；
 * 这里只作为面板卡死时的保护，不参与正常节拍。 */
#define EPD_BUSY_POLLS_LONG     4000000u   /* 4 s：复位 / LUT / 升压 / 全刷 */
#define EPD_BUSY_POLLS_SCAN     300000u    /* 300 ms：一次快刷扫描 */

/* 最近一次等待是否超时：面板卡住时上层可以做一次硬复位恢复 */
static uint8_t s_busy_timeout;
static uint8_t s_rtc_ready;
static uint32_t s_rtc_prescaler;

/* RTC 时基。LSI 在 Stop 下保持运行，RTC 因此可以作为睡眠期间的时间源。 */
static void epd_rtc_ensure(void)
{
    EXTI_InitTypeDef exti = {0};
    uint32_t guard = 0;

    if (s_rtc_ready != 0u) {
        return;
    }
    s_rtc_ready = 1;

    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR | RCC_HB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    /* 固定使用 LSI：板载 LSE 起振不可靠，一旦停振 RTC 就失去时钟，
     * RTC_WaitForLastTask() 会死等 RTOFF 把整机挂死。LSI 精度差一些但不会停。 */
    RCC_RTCCLKCmd(DISABLE);

    /* RTCSEL 一旦选定就被锁定，只有备份域复位才能重新选择。
     * 不先复位的话，之前锁在 LSE 的板子切不过去，RTC 会完全没有时钟。 */
    RCC_BackupResetCmd(ENABLE);
    RCC_BackupResetCmd(DISABLE);
    PWR_BackupAccessCmd(ENABLE);

    RCC_LSICmd(ENABLE);
    for (guard = 0; RCC_GetFlagStatus(RCC_FLAG_LSIRDY) != SET; ++guard) {
        if (guard >= EPD_LSI_WAIT_LOOPS) {
            break;
        }
    }

    s_rtc_prescaler = EPD_RTC_PRESCALER_LSI;
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    __asm__ volatile ("fence");
    RCC_RTCCLKCmd(ENABLE);

    RTC_WaitForLastTask();
    RTC_WaitForSynchro();

    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    RTC_ClearITPendingBit(RTC_IT_ALR);
    RTC_WaitForLastTask();

    RTC_EnterConfigMode();
    RTC_SetPrescaler(s_rtc_prescaler);
    RTC_WaitForLastTask();
    RTC_SetCounter(0);
    RTC_WaitForLastTask();
    RTC_ExitConfigMode();
    RTC_WaitForLastTask();

    /* RTC 闹钟走 EXTI17，既是 Stop 的定时唤醒源，也是 BUSY 超时的兜底 */
    exti.EXTI_Line = EXTI_Line17;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    NVIC_SetPriority(RTCAlarm_IRQn, 1);
    NVIC_EnableIRQ(RTCAlarm_IRQn);
}

/* RTC 计数器（1 ms 一格）。 */
uint32_t EPD_TickMs(void)
{
    return RTC_GetCounter();
}

void RTCAlarm_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void RTCAlarm_IRQHandler(void)
{
    /* RTC 闹钟也是 Stop 的唤醒源之一，同样先恢复时钟再处理标志 */
    epd_clock_restore();

    if (RTC_GetFlagStatus(RTC_FLAG_ALR) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line17);
        RTC_ClearITPendingBit(RTC_IT_ALR);
        RTC_WaitForLastTask();
    }
}

static void epd_rtc_alarm_off(void)
{
    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    EXTI_ClearITPendingBit(EXTI_Line17);
    RTC_ClearITPendingBit(RTC_IT_ALR);
    RTC_WaitForLastTask();
}

static void epd_rtc_alarm(uint32_t ms)
{
    if (ms == 0u) {
        ms = 1u;
    }

    epd_rtc_alarm_off();
    RTC_SetAlarm(RTC_GetCounter() + ms);
    RTC_WaitForLastTask();
    RTC_ITConfig(RTC_IT_ALR, ENABLE);
    RTC_WaitForLastTask();
}

vu32 tmp = 0,cfg = 0;


void PWR_STOPMode_LP_Cmd(FunctionalState NewState)
{
    uint32_t id = 0;
    id = (((*(uint32_t *)0x1FFFF704) & (0x000000F0)) >> 4);
    if(NewState != DISABLE)
    {
        switch(id)
        {
            case 0:
            {
                cfg &= ~((0x7 << 17));
                cfg |= ((0x1 << 17));
                break;
            }
            case 1:
            {
                cfg &= ~((0x7 << 17) | (0x7 << 4));
                cfg |= ((0x1 << 4));
                break;
            }
            case 2:
            {
                cfg &= ~((0x7 << 17) | (0x7 << 4));
                break;
            }
            default:
            {
                break;
            }
        }
        *(vu32*)SYS_CFGR0_BASE = cfg;
    }
    else
    {
        *(vu32*)SYS_CFGR0_BASE = tmp;
        Delay_Us(200);
    }
}

/* Stop 唤醒后系统时钟会切回 HSI，必须恢复 PLL，否则 SPI 分频后的实际速率会变。
 * 只有真的进了 Stop 才恢复：V5F 还在跑时系统没停时钟，SWS 仍然是 PLL。 */
static void epd_clock_restore(void)
{

#if 0
      PWR_STOPMode_LP_Cmd(DISABLE);
      SystemInit(); 
#else
    uint32_t guard;
    uint32_t tmp;

    if ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_HSI) {
        return;   /* 没真进 Stop，时钟仍是 PLL */
    }

    g_epd_phase = 4u;

    /* 官方 Stop 唤醒顺序：先把 VDDK 电压恢复并稳定，再恢复时钟。
     * PWR_EnterSTOPMode() 会置 LPDS 且退出时不自动清除，这里一并切回正常调压。 */
    PWR->CTLR &= ~(uint32_t)0x00000001u;
    epd_vddk_restore();
    for (guard = 0; guard < 400u; ++guard) {
        __asm__ volatile ("nop");
    }

    /* 与 SystemInit() 一致的寄存器前置准备：PLL 参考源 / SYSPLL 选择 / 中断标志都在这里，
     * 少这些位即使 PLLMUL/PLLSRC 写对了 PLL 也锁不住（实测表现为 PLLRDY 一直不来）。 */
    RCC->CTLR |= (uint32_t)0x00000001u;
    RCC->CFGR0 &= (uint32_t)0x305C0000u;
    guard = 0;
    while ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_HSI) {
        if (++guard >= EPD_WAIT_GUARD) {
            EPD_SetFault(EPD_FAULT_CLK_SW);
            g_epd_phase = 1u;
            return;
        }
    }
    RCC->CFGR0 &= (uint32_t)0xFFBFFFFFu;
    RCC->PLLCFGR &= (uint32_t)0x7FFFFFFFu;
    RCC->CTLR &= (uint32_t)0x6AA6FFFFu;
    RCC->CTLR &= (uint32_t)0xFFFBFFFFu;
    RCC->PLLCFGR &= (uint32_t)0x0FFFC000u;
    RCC->PLLCFGR |= (uint32_t)0x00000004u;
    RCC->INTR = 0x00FF0000u;
    RCC->CFGR2 &= 0x0C600000u;
    RCC->PLLCFGR2 &= 0xFFF0E080u;
    RCC->PLLCFGR2 |= 0x00080020u;

    /* PLL 时钟源 = HSI（25 MHz × 16 = 400 MHz）。
     * 用内部 HSI 后唤醒不需要等外部晶振起振，恢复时间从 ~1.5 ms 降到几十 us。 */
    RCC->PLLCFGR &= ~RCC_PLLMUL;
    RCC->PLLCFGR |= RCC_PLLMUL16;
    RCC->PLLCFGR &= ~RCC_PLL_SRC_DIV;
    RCC->PLLCFGR |= RCC_PLL_SRC_DIV1;
    RCC->PLLCFGR &= ~RCC_PLLSRC;
    RCC->PLLCFGR |= RCC_PLLSRC_HSI;
    guard = 0;
    while ((RCC->PLLCFGR & RCC_PLLSRC) != RCC_PLLSRC_HSI) {
        if (++guard >= EPD_WAIT_GUARD) {
            EPD_SetFault(EPD_FAULT_CLK_PLL);
            g_epd_phase = 1u;
            return;
        }
    }

    RCC->CTLR |= RCC_PLLON;
    guard = 0;
    while ((RCC->CTLR & RCC_PLLRDY) == 0u) {
        if (++guard >= EPD_WAIT_GUARD) {
            EPD_SetFault(EPD_FAULT_CLK_PLL);
            g_epd_phase = 1u;
            return;
        }
    }

    /* 打开 PLL 输出门，并把 SYSPLL 源选回 PLL_CLK */
    RCC->PLLCFGR &= ~RCC_SYSPLL_GATE;
    RCC->PLLCFGR &= ~RCC_SYSPLL_SEL;
    guard = 0;
    while ((RCC->PLLCFGR & RCC_SYSPLL_SEL) != 0u) {
        if (++guard >= EPD_WAIT_GUARD) {
            EPD_SetFault(EPD_FAULT_CLK_PLL);
            g_epd_phase = 1u;
            return;
        }
    }

    /* 恢复分频与 FLASH 等待周期，最后再切 SYSCLK */
    RCC->CFGR0 &= ~RCC_HPRE;
    RCC->CFGR0 |= RCC_HPRE_DIV1;
    RCC->CFGR0 &= ~RCC_FPRE;
    RCC->CFGR0 |= RCC_FPRE_DIV4;
    tmp = FLASH->ACTLR;
    tmp &= ~(uint32_t)0x3u;
    tmp |= FLASH_ACTLR_LATENCY_HCLK_DIV2;
    FLASH->ACTLR = tmp;

    RCC->PLLCFGR |= RCC_SYSPLL_GATE;
    RCC->CFGR0 &= ~RCC_SW;
    RCC->CFGR0 |= RCC_SW_PLL;
    guard = 0;
    while ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_PLL) {
        if (++guard >= EPD_WAIT_GUARD) {
            EPD_SetFault(EPD_FAULT_CLK_SW);
            g_epd_phase = 1u;
            return;
        }
    }

    SystemAndCoreClockUpdate();
    /* 不再调用 epd_hw_init()：Stop 只是停时钟，SPI/GPIO/DMA 寄存器内容都还在，
     * 唤醒后分频一样；而它会在面板刷新途中重配 SPI/DMA 并拨动 RST，破坏画面。 */
    g_epd_phase = 1u;
#endif
}

/* 只做一次 Stop 睡眠，闹钟的装/卸由调用方负责。
 * 关键：一次等待只装一次闹钟。若每次睡眠前都重装，可能出现"闹钟在进入 WFI 之前
 * 就已经触发、被 ISR 清掉，随后又带着已消耗的闹钟进入 Stop"——那就再也醒不过来，
 * 表现就是跑一段时间后整芯片停在 Stop、连 SWD 都不响应。 */
static void epd_stop_once(void)
{
    g_epd_phase = 5u;
    GPIO_ResetBits(GPIOB, GPIO_Pin_1);
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    epd_clock_restore();
    GPIO_SetBits(GPIOB, GPIO_Pin_1);
}

/* 睡 ms 毫秒：装一次闹钟 + 一次 Stop */
static void epd_stop_ms(uint32_t ms)
{
    if (ms == 0u) {
        ms = 1u;
    }
    g_epd_phase = 6u;
    epd_rtc_alarm(ms);
    epd_stop_once();
    epd_rtc_alarm_off();
}
/* 低功耗等待：任意中断都会把 CPU 叫醒，适合主循环的空闲等待。 */
void EPD_SleepUs(uint32_t us)
{
    epd_stop_ms((us + 999u) / 1000u);
}

static void epd_wait(uint32_t timeout_us)
{
    const uint32_t timeout_ms = (timeout_us + 999u) / 1000u;
    uint32_t deadline;
    uint32_t rounds = 0u;

    epd_rtc_ensure();
    NVIC_DisableIRQ(IPC_CH2_IRQn);

    if (BUSY() == 0) {
        NVIC_EnableIRQ(IPC_CH2_IRQn);
        return;
    }

    /* 整个等待只装一次闹钟：既当超时兜底，也避免每次睡眠重装闹钟的丢唤醒竞态。
     * 正常唤醒靠 BUSY 的下降沿中断，闹钟只在面板卡住时兜底。 */
    g_epd_phase = 6u;
    epd_rtc_alarm(timeout_ms);
    deadline = EPD_TickMs() + timeout_ms;

    while (BUSY()) {
        const int32_t remain = (int32_t)(deadline - EPD_TickMs());

        if (remain <= 0) {
            s_busy_timeout = 1u;
            g_epd_phase = 7u;
            EPD_SetFault(EPD_FAULT_BUSY);
            PRINT("[EPD] busy timeout\r\n");
            break;
        }

        epd_stop_once();

        /* 兜底：万一 BUSY 沿和闹钟都没能把我们叫醒并正常退出，也不要无限循环 */
        if (++rounds > ((timeout_ms / 10u) + 20u)) {
            s_busy_timeout = 1u;
            EPD_SetFault(EPD_FAULT_BUSY);
            break;
        }
    }

    epd_rtc_alarm_off();
    NVIC_EnableIRQ(IPC_CH2_IRQn);
}

/* -------------------------------------------------------------- helpers */

static void epd_analog_off(void)
{
    if (!analog_on) {
        return;
    }
    EPD_Cmd(0x22);
    EPD_Dat(0x03);
    EPD_Cmd(0x20);
    epd_wait(EPD_BUSY_POLLS_LONG);
    if (!BUSY()) {
        analog_on = 0;
    }
}

static void epd_analog_on(void)
{
    EPD_Cmd(0x22);
    EPD_Dat(0xC0);
    EPD_Cmd(0x20);
    epd_wait(EPD_BUSY_POLLS_LONG);
    if (!BUSY()) {
        analog_on = 1;
    }
}

static void epd_reset(void)
{
    epd_analog_off();
    RST_H();
    EPD_DelayMs(EPD_RESET_HIGH_MS);
    RST_L();
    EPD_DelayMs(EPD_RESET_LOW_MS);
    RST_H();
    EPD_DelayMs(EPD_RESET_HIGH_MS);
    analog_on = 0;
    s_pending = 0;
    epd_wait(EPD_BUSY_POLLS_LONG);
}

static void epd_load_lut(const uint8_t *lut)
{
    EPD_Cmd(0x32);
    epd_dat_buf(lut, 153);
    epd_wait(EPD_BUSY_POLLS_LONG);
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x3F);
    EPD_Dat(lut[153]);
    EPD_Cmd(0x03);
    EPD_Dat(lut[154]);
    EPD_Cmd(0x04);
    EPD_Dat(lut[155]);
    EPD_Dat(lut[156]);
    EPD_Dat(lut[157]);
    EPD_Cmd(0x2C);
    EPD_Dat(lut[158]);
}

/* 设置 RAM 窗口。行优先缓存按 X、Y 递减的顺序直接写入，
 * 使逻辑像素映射与 SHT_152 已验证的列优先驱动一致。
 */
static void epd_window(uint8_t row0, uint8_t row1, uint8_t col0, uint8_t col1)
{
    const uint8_t x0 = (uint8_t)(EPD_STRIDE - 1 - col0);
    const uint8_t x1 = (uint8_t)(EPD_STRIDE - 1 - col1);
    const uint8_t y0 = (uint8_t)(EPD_H - 1 - row0);
    const uint8_t y1 = (uint8_t)(EPD_H - 1 - row1);

    EPD_Cmd(0x44);              /* X 起止（字节列），递减 */
    EPD_Dat(x0);
    EPD_Dat(x1);
    EPD_Cmd(0x45);              /* Y 起止，递减 */
    EPD_Dat(y0);
    EPD_Dat(0x00);
    EPD_Dat(y1);
    EPD_Dat(0x00);
    EPD_Cmd(0x4E);              /* 光标 X = 起始列 */
    EPD_Dat(x0);
    EPD_Cmd(0x4F);              /* 光标 Y = 起始行 */
    EPD_Dat(y0);
    EPD_Dat(0x00);
}

static void epd_window_full(void)
{
    epd_window(0, (uint8_t)(EPD_H - 1), 0, (uint8_t)(EPD_STRIDE - 1));
}

/* --------------------------------------------------------------- public */

void EPD_Init(void)
{
    epd_reset();
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x12);
    EPD_DelayMs(10);
    epd_wait(EPD_BUSY_POLLS_LONG);
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x01);
    EPD_Dat(0x97);
    EPD_Dat(0x00);
    EPD_Dat(0x00);

    EPD_Cmd(0x11);
    EPD_Dat(0x00);

    epd_window_full();

    EPD_Cmd(0x3C);
    EPD_Dat(0x01);

    EPD_Cmd(0x18);
    EPD_Dat(0x80);
}

void EPD_SendDisplay(const uint8_t *image)
{
    EPD_FastSync();
    if (s_pending) {
        return;
    }

    epd_reset();
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x12);
    EPD_DelayMs(10);
    epd_wait(EPD_BUSY_POLLS_LONG);
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x01);
    EPD_Dat(0x97);
    EPD_Dat(0x00);
    EPD_Dat(0x00);
    EPD_Cmd(0x11);
    EPD_Dat(0x00);
    epd_window_full();
    EPD_Cmd(0x3C);
    EPD_Dat(0x01);
    EPD_Cmd(0x18);
    EPD_Dat(0x80);

    EPD_Cmd(0x22);          /* load temperature */
    EPD_Dat(0xB1);
    EPD_Cmd(0x20);
    epd_wait(EPD_BUSY_POLLS_LONG);
    if (BUSY()) {
        return;
    }

    epd_load_lut(lut_full);
    if (BUSY()) {
        return;
    }

    epd_window_full();
    EPD_Cmd(0x24);
    epd_dat_buf(image, EPD_FRAME_BYTES);

    epd_window_full();
    EPD_Cmd(0x26);
    epd_dat_buf(image, EPD_FRAME_BYTES);

    EPD_Cmd(0x22);
    EPD_Dat(0xC7);
    EPD_Cmd(0x20);
    epd_wait(EPD_BUSY_POLLS_LONG);      /* 全刷一次约 1.3 秒 */
    if (!BUSY()) {
        analog_on = 0;      /* the full sequence powers down by itself */
        memset(s_net, 0, EPD_PIXELS);
        memset(s_net_next, 0, EPD_PIXELS);
        memset(s_active, 0x01, EPD_FRAME_BYTES);
        s_planned_count = 0;
    }
}

void EPD_FastPrepareFrames(uint8_t frames)
{
    static uint8_t scratch[159];

    s_fast_frames = (frames != 0u) ? frames : 1u;
    memset(s_active, 0x01, EPD_FRAME_BYTES);
    for (uint16_t i = 0; i < 159u; ++i) {
        scratch[i] = lut_fast_base[i];
    }
    if (frames <= 1u) {
        scratch[60] = 0;
        scratch[68] = 0;
    } else if (frames == 2u) {
        scratch[60] = 0;
        scratch[68] = 1;
    } else {
        scratch[60] = (uint8_t)(frames - 2u);
        scratch[68] = 1;
    }

    epd_reset();            /* hardware reset only: the partial path skips 0x12 */
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x01);
    EPD_Dat(0x97);
    EPD_Dat(0x00);
    EPD_Dat(0x00);
    EPD_Cmd(0x11);
    EPD_Dat(0x00);
    EPD_Cmd(0x18);
    EPD_Dat(0x80);

    epd_load_lut(scratch);
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x37);          /* no automatic RAM ping-pong */
    for (uint8_t i = 0; i < 10u; ++i) {
        EPD_Dat(0x00);
    }

    EPD_Cmd(0x3C);
    EPD_Dat(0x80);

    epd_analog_on();        /* power up once and keep it up */
}

void EPD_FastPrepare(void)
{
    EPD_FastPrepareFrames(EPD_FAST_FRAMES);
}

/* 与 budget 驱动共用接口；在 net 驱动中表示黑/白净驱动上下限。 */
void EPD_SetPulseBudget(uint8_t black, uint8_t white)
{
    if (black == 0u) {
        black = 1u;
    } else if (black > 127u) {
        black = 127u;
    }
    if (white == 0u) {
        white = 1u;
    } else if (white > 127u) {
        white = 127u;
    }
    s_dc_black = black;
    s_dc_white = white;
    memset(s_active, 0x01, EPD_FRAME_BYTES);
}

/* 帧率限制：BUSY 落下后额外等待的微秒数。0 = 不限速。 */
static uint32_t s_hold_us = EPD_FRAME_HOLD_US;

void EPD_SetFrameHoldUs(uint32_t us)
{
    s_hold_us = us;
}

/* 按目标帧率算需要的 hold（粗略，busy/spi 用默认值估） */
void EPD_SetTargetFps(uint16_t fps)
{
    const uint32_t period_us = (fps != 0u) ? (1000000u / fps) : 0u;
    const uint32_t fixed_us = EPD_FRAME_FIXED_US;

    s_hold_us = (period_us > fixed_us) ? (period_us - fixed_us) : 0u;
}


// 传入一帧并刷新
// black和white指向对应的数据数组
void EPD_FastScan(const uint8_t *black, const uint8_t *white)
{
    EPD_FastSync();
    if (s_pending) {
        return;
    }

    epd_window_full();
    EPD_Cmd(0x26);
    epd_dat_buf(black, EPD_FRAME_BYTES);

    EPD_Cmd(0x24);
    epd_dat_buf(white, EPD_FRAME_BYTES);

    EPD_Cmd(0x22);
    EPD_Dat(0x0C);
    EPD_Cmd(0x20);
    epd_wait(EPD_BUSY_POLLS_SCAN);
}

// 核心函数：按目标色驱动，并把每像素净驱动帧数限制在 [-7,+7]。
static int epd_plan_frame(const uint8_t *next, uint8_t *r0, uint8_t *r1)
{
    const int hi = (int)s_dc_black;
    const int lo = -(int)s_dc_white;
    uint8_t first = EPD_H;
    uint8_t last = 0;
    uint16_t planned = 0;
    int driven = 0;

    for (uint32_t row = 0; row < EPD_H; ++row) {
        const uint32_t base = row * EPD_STRIDE;
        const uint32_t pbase = row * EPD_W;
        int any = 0;

        for (uint32_t k = 0; k < EPD_STRIDE; ++k) {
            const uint32_t index = base + k;
            const uint32_t pixel = pbase + k * 8u;
            const uint8_t px = next[index];
            uint8_t mb = 0;
            uint8_t mw = 0;
            uint8_t next_active = 0;

            if ((s_active[index] != 0u) || (px != s_target[index])) {
                s_planned_pixels[planned++] = (uint16_t)pixel;
                for (uint8_t b = 0; b < 8u; ++b) {
                    const uint8_t m = (uint8_t)(0x80u >> b);
                    const uint32_t p = pixel + b;
                    const int d = ((px & m) != 0u) ? -(int)s_fast_frames : (int)s_fast_frames;
                    const int old = (int)s_net[p];
                    const int v = old + d;
                    int committed = old;

                    if ((v >= lo) && (v <= hi)) {
                        if (d > 0) {
                            mb = (uint8_t)(mb | m);
                        } else {
                            mw = (uint8_t)(mw | m);
                        }
                        any = 1;
                        committed = v;
                    }
                    s_net_next[p] = (int8_t)committed;
                    if ((committed + d >= lo) && (committed + d <= hi)) {
                        next_active = 1;
                    }
                }
            }

            s_active_next[index] = next_active;
            s_mask_black[index] = mb;
            s_mask_white[index] = mw;
        }

        if (any != 0) {
            if (row < first) {
                first = (uint8_t)row;
            }
            last = (uint8_t)row;
            driven = 1;
        }
    }

    s_planned_count = planned;
    *r0 = first;
    *r1 = last;

    return driven;
}

static void epd_commit_plan(const uint8_t *target)
{
    for (uint16_t i = 0; i < s_planned_count; ++i) {
        const uint32_t p = s_planned_pixels[i];
        memcpy(&s_net[p], &s_net_next[p], 8u);
    }
    memcpy(s_active, s_active_next, EPD_FRAME_BYTES);
    memcpy(s_target, target, EPD_FRAME_BYTES);
}

static void epd_discard_plan(void)
{
    for (uint16_t i = 0; i < s_planned_count; ++i) {
        const uint32_t p = s_planned_pixels[i];
        memcpy(&s_net_next[p], &s_net[p], 8u);
    }
}

/* 发一个平面：设窗口 -> 命令 -> 整块 DMA。掩码已经是连续的行数据，
 * 所以只要把窗口那一段的指针递过去就行，一次 DMA 传完。 */
static void epd_send_one_plane(uint8_t cmd, const uint8_t *mask,
                               uint8_t row0, uint8_t row1, uint8_t col0, uint8_t col1)
{
    const uint32_t first = (uint32_t)row0 * EPD_STRIDE;
    const uint32_t bytes = (uint32_t)(row1 - row0 + 1u) * EPD_STRIDE;

    epd_window(row0, row1, col0, col1);
    EPD_Cmd(cmd);
    epd_dat_buf(&mask[first], bytes);
}

/* 等面板把已经激活的那一帧刷完。
   如果是单核运行的话，激活之后
   CPU可以先算下一帧。一边算面板一边刷新
   刷完了再回来。 */
void EPD_FastSync(void)
{
    if (!s_pending) {
        return;
    }
    epd_wait(EPD_BUSY_POLLS_SCAN); //有超时的等待busy
    if (BUSY()) {
        //等待完了如果还是忙说明卡死，epd_wait会把超时标志置位，这里返回就行了
        return;
    }
    s_pending = 0;
}

static void epd_send_planes(uint8_t row0, uint8_t row1, uint8_t col0, uint8_t col1)
{
    epd_send_one_plane(0x26, s_mask_black, row0, row1, col0, col1);   /* 黑掩码 -> 0x26 */
    epd_send_one_plane(0x24, s_mask_white, row0, row1, col0, col1);   /* 白掩码 -> 0x24 */
}

// 激活刷新，0x0C会让epd的升压电路不关闭，这样才能做到很快地刷新 
static void epd_activate(void)
{
    EPD_Cmd(0x22);
    EPD_Dat(0x0C);          /* display, one controller frame, keep the booster */
    EPD_Cmd(0x20);
    s_pending = 1;
}

void EPD_FastBegin(const uint8_t *shown)
{
    (void)shown;
    memset(s_net, 0, EPD_PIXELS);
    memset(s_net_next, 0, EPD_PIXELS);
    memset(s_target, 0, EPD_FRAME_BYTES);
    memset(s_active, 0x01, EPD_FRAME_BYTES);
    s_planned_count = 0;
    s_scan_count = 0;
    s_hold_pending = 0;
    s_pending = 0;
    s_row0 = EPD_H;
    s_row1 = 0;
}

/* 取走"上一次 FastFrame 是重来否遇到 BUSY 超时"的标志（读后清零）。
 * 上层看到 1 就说明面板卡住了，应该 EPD_Init() 一遍。 */
uint8_t EPD_TakeTimeoutFlag(void)
{
    const uint8_t t = s_busy_timeout;

    s_busy_timeout = 0;
    return t;
}

uint32_t EPD_FastScanCount(void)
{
    return s_scan_count;
}

//主函数，外部不断调用这里。
int EPD_FastFrame(const uint8_t *next)
{
    uint8_t row0, row1;
    int driven = 0;

    // 阶段清零，每隔一段时间全刷 
    if ((EPD_CLEAN_INTERVAL != 0) && (s_scan_count >= (uint32_t)EPD_CLEAN_INTERVAL)) {
        PRINT("[EPD] clean refresh after %u fast scans\r\n", (unsigned)s_scan_count);
        EPD_SendDisplay(next);
        if (s_busy_timeout) {
            return -1;
        }
        EPD_FastBegin(next);
        EPD_FastPrepare();
        return s_busy_timeout ? -1 : 1;
    }

    /* 无驱动时只写局部行号，保留上一次有效范围供 HOLD 收尾使用。 */
    driven = epd_plan_frame(next, &row0, &row1);

    /* 列范围固定整行（0..18）。列也缩小范围AI没搞出来。
    不过之前light ink做到过，所以肯定是有办法的
    不过这个只影响写入RAM的速度，作用不算很大，不浪费时间了 */
    s_col0 = 0;
    s_col1 = (uint8_t)(EPD_STRIDE - 1);

    if (driven) {
        /* busy高的时候不能写RAM，所以还是得等刷完了再写进去 */
        EPD_FastSync();      // 等上一帧刷完
        if (s_pending) {
            epd_discard_plan();
            return -1;
        }
        s_row0 = row0;
        s_row1 = row1;

        /* 限速：BUSY 落下之后先等一会儿再写下一帧，把帧率压到目标值。
         * 帧周期 = busy + hold + spi + mask，hold = 11000 µs 时约 30 fps。 */
        if (s_hold_us != 0u) {
            EPD_DelayUs(s_hold_us);
        }

        //把待刷新的数据写入EPD中
#if EPD_PARTIAL_WINDOW
        epd_send_planes(s_row0, s_row1, s_col0, s_col1);          /* 只发变化的行 */
#else
        epd_send_planes(0, (uint8_t)(EPD_H - 1), 0, (uint8_t)(EPD_STRIDE - 1));
#endif
        //刷新动作就是这个函数。触发刷新之后对像素预算进行处理就很快返回了。如果单核的话此时主循环就可以去算下一帧。
        epd_activate();

        ++s_scan_count;
        s_hold_pending = 1;
        epd_commit_plan(next);
    } else if (EPD_HOLD_ENDING && s_hold_pending) {
        /* HOLD 收尾：沿用上一次真正发过的行范围，两块全 0 掩码，所有像素都不驱动，
         * 让上一次的驱动把波形走完而不是停在半途。 */
        if (s_row1 >= s_row0) {
            EPD_FastSync();          /* 等上一帧刷完 */
            if (s_pending) {
                epd_discard_plan();
                return -1;
            }
            epd_send_one_plane(0x26, s_mask_black, s_row0, s_row1, s_col0, s_col1);
            epd_send_one_plane(0x24, s_mask_black, s_row0, s_row1, s_col0, s_col1);
            epd_activate();          /* 全 HOLD 的一帧，同样不等它刷 */

            ++s_scan_count;
        }
        s_hold_pending = 0;
    }

    if (!driven) {
        epd_commit_plan(next);
    }
    return driven;
}

void EPD_Sleep(void)
{
    EPD_FastSync();
    if (s_pending) {
        return;
    }
    epd_analog_off();
    if (BUSY()) {
        return;
    }

    EPD_Cmd(0x10);
    EPD_Dat(0x01);
    EPD_DelayMs(100);
}

