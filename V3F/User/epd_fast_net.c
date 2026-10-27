/* ============================================================================
 * SSD1680 152x152 电子纸高刷驱动 —— CH32H417（跑在 V3F 上）
 *
 * 换单片机时只需要改两处：
 *   1. 下面的 #include（board.h 里选 SDK 头文件）
 *   2. 紧跟其后的「移植区」：引脚宏、SPI/DMA 参数、时基、收发原语、EPD_Hal_Init()
 * 移植区之后是纯驱动逻辑，和具体单片机无关。
 * ==========================================================================*/

#include "ch32h417.h"
#include "print.h"
#include "epd_fast.h"

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

/* ----------------------------SPI收发--------------------------------  */

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
    uint32_t guard = 2000000u;

    DMA_Cmd(EPD_DMA_CHANNEL, DISABLE);
    EPD_DMA_TC_CLEAR();
    EPD_DMA_CHANNEL->MADDR = (uint32_t)buf;
    DMA_SetCurrDataCounter(EPD_DMA_CHANNEL, (uint16_t)len);
    DMA_Cmd(EPD_DMA_CHANNEL, ENABLE);

    while (!EPD_DMA_TC_READY()) {
        if (--guard == 0u) {
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
    }

    /* DMA 完成时最后一个字节可能还在移位寄存器里 */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
    }
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) {
    }

    DMA_Cmd(EPD_DMA_CHANNEL, DISABLE);
    EPD_DMA_TC_CLEAR();

    /* SPI传完要读一下接收 */
    (void)SPI1->DATAR;
    (void)SPI1->STATR;
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

void EPD_Hal_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    SPI_InitTypeDef spi = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA |
                          RCC_HB2Periph_GPIOC | RCC_HB2Periph_GPIOE, ENABLE);

    /* CS / DC / RST 输出 */
    gpio.GPIO_Pin = EPD_CS_PIN | EPD_DC_PIN | EPD_RST_PIN;
    gpio.GPIO_Speed = EPD_GPIO_SPEED;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(EPD_CS_PORT, &gpio);

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
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
}

/* BUSY 引脚的 EXTI 中断：只清标志，作用是让 epd_wait() 里的 WFI 醒过来。
 * 真正的判断还在 epd_wait() 里读引脚，所以这里不做任何业务逻辑。 */
void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void)
{
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
static uint8_t s_mask_black[EPD_FRAME_BYTES];
static uint8_t s_mask_white[EPD_FRAME_BYTES];

static uint32_t s_scan_count;   /* fast scans since the last full refresh */
static uint8_t  s_pending;      /* 已经激活、面板还在刷 */
static uint8_t  s_row0, s_row1; /* 本帧要发的行范围（局部刷新用） */
static uint8_t  s_col0, s_col1; /* 本帧要发的列范围（字节列 0..18） */
static uint8_t s_hold_pending;  /* a scan drove something and still needs its HOLD ending */

/* ------------------------------------------------------------ 等待 BUSY
 *
 * EPD_WFI = 1：BUSY 高电平期间进 WFI，BUSY 下降沿（一次波形结束）通过 EXTI 唤醒。
 * 一帧里大约 20 ms 都在这里睡，是最省电的一段。
 *
 * 超时用 SysTick0 的比较中断做，不再靠轮询次数累加：
 *   - SysTick0 同时是系统的自由运行时基（CNT 只受计数器影响，不受 CMP/IE 影响）
 *   - 每次只装一个一次性比较值，醒来后马上关掉比较中断
 *   - 任意中断（BUSY、IPC、其它）都能提前唤醒，外层重新判断并重装
 */
#define EPD_WFI                 1

/* BUSY 超时（单位：微秒）。正常快刷扫描 20~26 ms，全刷约 1.3 s；
 * 这里只作为面板卡死时的保护，不参与正常节拍。 */
#define EPD_BUSY_POLLS_LONG     4000000u   /* 4 s：复位 / LUT / 升压 / 全刷 */
#define EPD_BUSY_POLLS_SCAN     300000u    /* 300 ms：一次快刷扫描 */

/* 最近一次等待是否超时：面板卡住时上层可以做一次硬复位恢复 */
static uint8_t s_busy_timeout;

void SysTick0_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick0_Handler(void)
{
    if ((SysTick0->ISR & 1u) != 0u) {
        SysTick0->ISR &= ~1u;
    }
}

/* HCLK 计数换算成 SysTick 计数 */
static uint32_t epd_us_to_ticks(uint32_t us)
{
    uint64_t ticks = (uint64_t)us * HCLKClock / 1000000u;

    if (ticks == 0u) {
        ticks = 1u;
    }
    return (uint32_t)ticks;
}

static void epd_timer_ensure(void)
{
    static uint8_t ready;

    if (ready != 0u) {
        return;
    }
    ready = 1;

    SysTick0->CTLR |= (1u << 2) | (1u << 0);   /* HCLK 源 + 使能 */
    SysTick0->ISR &= ~1u;
    NVIC_ClearPendingIRQ(SysTick0_IRQn);
    NVIC_SetPriority(SysTick0_IRQn, 1);
    NVIC_EnableIRQ(SysTick0_IRQn);
}

/* WFI 到 ticks 个 HCLK 之后，或者被任意中断提前叫醒。
 * 提前醒没关系，调用方会重新判断条件、按剩余时间重装。 */
static void epd_sleep_ticks(uint32_t ticks)
{
    if (ticks == 0u) {
        ticks = 1u;
    }

    epd_timer_ensure();

    SysTick0->ISR &= ~1u;
    NVIC_ClearPendingIRQ(SysTick0_IRQn);
    SysTick0->CMP = SysTick0->CNT + ticks;
    SysTick0->CTLR |= (1u << 1);               /* 打开比较中断 */
    __WFI();
    SysTick0->CTLR &= ~(1u << 1);
    SysTick0->ISR &= ~1u;
}

/* 低功耗等待：任意中断都会把 CPU 叫醒，适合主循环的空闲等待。 */
void EPD_SleepUs(uint32_t us)
{
    epd_sleep_ticks(epd_us_to_ticks(us));
}

static void epd_wait(uint32_t timeout_us)
{
    uint32_t deadline;

    epd_timer_ensure();
    deadline = SysTick0->CNT + epd_us_to_ticks(timeout_us);

    while (BUSY()) {
        const int32_t remain = (int32_t)(deadline - SysTick0->CNT);

        if (remain <= 0) {
            s_busy_timeout = 1;
            PRINT("[EPD] busy timeout\r\n");
            return;
        }
        epd_sleep_ticks((uint32_t)remain);
    }
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
        for (uint32_t i = 0; i < EPD_PIXELS; ++i) {
            s_net[i] = 0;
        }
    }
}

void EPD_FastPrepareFrames(uint8_t frames)
{
    static uint8_t scratch[159];

    s_fast_frames = (frames != 0u) ? frames : 1u;
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
    int driven = 0;

    for (uint32_t row = 0; row < EPD_H; ++row) {
        const uint32_t base = row * EPD_STRIDE;
        const uint32_t pbase = row * EPD_W;
        int any = 0;

        for (uint32_t k = 0; k < EPD_STRIDE; ++k) {
            const uint8_t px = next[base + k];
            uint8_t mb = 0;
            uint8_t mw = 0;

            for (uint8_t b = 0; b < 8u; ++b) {
                const uint8_t m = (uint8_t)(0x80u >> b);
                const uint32_t p = pbase + k * 8u + b;
                const int d = ((px & m) != 0u) ? -(int)s_fast_frames : (int)s_fast_frames;
                const int v = (int)s_net[p] + d;

                if ((v >= lo) && (v <= hi)) {
                    if (d > 0) {
                        mb = (uint8_t)(mb | m);
                    } else {
                        mw = (uint8_t)(mw | m);
                    }
                    any = 1;
                }
            }

            s_mask_black[base + k] = mb;
            s_mask_white[base + k] = mw;
        }

        if (any != 0) {
            if (row < first) {
                first = (uint8_t)row;
            }
            last = (uint8_t)row;
            driven = 1;
        }
    }

    *r0 = first;
    *r1 = last;

    return driven;
}

/* 扫描已经交给面板后，按实际掩码更新净驱动帧数。 */
static void epd_apply_charge(void)
{
    for (uint32_t row = 0; row < EPD_H; ++row) {
        const uint32_t base = row * EPD_STRIDE;
        const uint32_t pbase = row * EPD_W;

        for (uint32_t k = 0; k < EPD_STRIDE; ++k) {
            const uint8_t mb = s_mask_black[base + k];
            const uint8_t mw = s_mask_white[base + k];

            if ((uint8_t)(mb | mw) == 0u) {
                continue;
            }
            for (uint8_t b = 0; b < 8u; ++b) {
                const uint8_t m = (uint8_t)(0x80u >> b);
                const uint32_t p = pbase + k * 8u + b;

                if ((mb & m) != 0u) {
                    s_net[p] = (int8_t)(s_net[p] + (int)s_fast_frames);
                } else if ((mw & m) != 0u) {
                    s_net[p] = (int8_t)(s_net[p] - (int)s_fast_frames);
                }
            }
        }
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
    for (uint32_t i = 0; i < EPD_PIXELS; ++i) {
        s_net[i] = 0;
    }
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

        /* 这一帧已经交给面板了，更新每像素净驱动帧数。 */
        epd_apply_charge();
    } else if (EPD_HOLD_ENDING && s_hold_pending) {
        /* HOLD 收尾：沿用上一次真正发过的行范围，两块全 0 掩码，所有像素都不驱动，
         * 让上一次的驱动把波形走完而不是停在半途。 */
        if (s_row1 >= s_row0) {
            const uint32_t first = (uint32_t)s_row0 * EPD_STRIDE;
            const uint32_t bytes = (uint32_t)(s_row1 - s_row0 + 1u) * EPD_STRIDE;

            for (uint32_t i = 0; i < bytes; ++i) {
                s_mask_black[first + i] = 0x00;
            }

            EPD_FastSync();          /* 等上一帧刷完 */
            if (s_pending) {
                return -1;
            }
            epd_send_one_plane(0x26, s_mask_black, s_row0, s_row1, s_col0, s_col1);
            epd_send_one_plane(0x24, s_mask_black, s_row0, s_row1, s_col0, s_col1);
            epd_activate();          /* 全 HOLD 的一帧，同样不等它刷 */

            ++s_scan_count;
        }
        s_hold_pending = 0;
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

