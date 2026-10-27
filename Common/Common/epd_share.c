#include "epd_share.h"

#if defined(Core_V5F) || defined(Core_V3F)

/* 帧握手用的两条单向 IPC 通道（和 printf 的 CH0 分开）：
 *   CH1 bit0  V3F -> V5F  "允许产下一帧"，接收中断给 V5F
 *   CH2 bit0  V5F -> V3F  "新帧已提交"，接收中断给 V3F
 * IPC 控制寄存器是两核共享的，所以每个核只配置自己要接收的那条通道。 */
#define SHARE_CMD_CH     IPC_CH1
#define SHARE_CMD_BIT    IPC_CH_Sta_Bit0
#define SHARE_RDY_CH     IPC_CH2
#define SHARE_RDY_BIT    IPC_CH_Sta_Bit0

static inline void epd_share_fence(void)
{
    __asm__ volatile ("fence rw, rw" ::: "memory");
}

static void epd_share_ipc_init(void)
{
    IPC_InitTypeDef ipc = {0};

#if defined(Core_V5F)
    /* 只收 V3F 的“可以产帧”通知：发送端 V3F(0)，接收中断给 V5F(1)。 */
    IPC_StructInit(&ipc);
    ipc.IPC_CH = SHARE_CMD_CH;
    ipc.TxCID = IPC_TxCID0;
    ipc.RxCID = IPC_RxCID1;
    ipc.TxIER = DISABLE;
    ipc.RxIER = ENABLE;
    ipc.AutoEN = DISABLE;
    IPC_Init(&ipc);

    IPC_ClearFlagStatus(SHARE_CMD_CH, SHARE_CMD_BIT);
    IPC_ITConfig(SHARE_CMD_CH, SHARE_CMD_BIT, ENABLE);
    NVIC_SetPriority(IPC_CH1_IRQn, 1);
    NVIC_EnableIRQ(IPC_CH1_IRQn);
#elif defined(Core_V3F)
    /* 只收 V5F 的“新帧已提交”通知：发送端 V5F(1)，接收中断给 V3F(0)。 */
    IPC_StructInit(&ipc);
    ipc.IPC_CH = SHARE_RDY_CH;
    ipc.TxCID = IPC_TxCID1;
    ipc.RxCID = IPC_RxCID0;
    ipc.TxIER = DISABLE;
    ipc.RxIER = ENABLE;
    ipc.AutoEN = ENABLE;
    IPC_Init(&ipc);

    IPC_ClearFlagStatus(SHARE_RDY_CH, SHARE_RDY_BIT);
    IPC_ITConfig(SHARE_RDY_CH, SHARE_RDY_BIT, ENABLE);
    NVIC_SetPriority(IPC_CH2_IRQn, 1);
    NVIC_EnableIRQ(IPC_CH2_IRQn);
#endif
}

void epd_share_init(void)
{
    static uint8_t s_ipc_ready;

    if (s_ipc_ready == 0u) {
        s_ipc_ready = 1;
        epd_share_ipc_init();
    }

#if defined(Core_V3F)
    /* V3F 是主核，每次启动都重建共享块。调试器复位不会清 RAM，
     * 残留的 magic/序号/统计会让首帧握手错位，也会让 drop 计数看起来很大。 */
    EPD_SHARE->write_seq = 0;
    EPD_SHARE->read_seq = 0;
    EPD_SHARE->dropped = 0;
    EPD_SHARE->prod_fps = 0;
    EPD_SHARE->cons_fps = 0;
    epd_share_fence();
    EPD_SHARE->magic = EPD_SHARE_MAGIC;
#else
    // V5F 只做兜底清理；正常启动时 V3F 已经把暗号（magic）设置好了
    if (EPD_SHARE->magic != EPD_SHARE_MAGIC) {
        EPD_SHARE->write_seq = 0;
        EPD_SHARE->read_seq = 0;
        EPD_SHARE->dropped = 0;
        EPD_SHARE->prod_fps = 0;
        EPD_SHARE->cons_fps = 0;
        epd_share_fence();
        EPD_SHARE->magic = EPD_SHARE_MAGIC;
    }
#endif
}

#if defined(Core_V5F)
/* 只是把 WFI 叫醒；真正的等待条件在 epd_share_wait_consumed() 里查。 */
void IPC_CH1_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void IPC_CH1_Handler(void)
{
    if (IPC_GetITStatus(SHARE_CMD_CH, SHARE_CMD_BIT) != RESET) {
        IPC_ClearFlagStatus(SHARE_CMD_CH, SHARE_CMD_BIT);
    }
}
#elif defined(Core_V3F)
/* 只是把 WFI 叫醒；真正的帧数据在 epd_share_peek() 里取。 */
void IPC_CH2_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void IPC_CH2_Handler(void)
{
    if (IPC_GetITStatus(SHARE_RDY_CH, SHARE_RDY_BIT) != RESET) {
        IPC_ClearFlagStatus(SHARE_RDY_CH, SHARE_RDY_BIT);
    }
}
#endif

#if defined(Core_V5F)

uint8_t *epd_share_begin(void)
{
    /* 正常情况下每帧都先等 V3F 取走，待消费帧数最多 1，这里不会失败。
     * 保留溢出检查，避免异常时序下覆盖还没显示完的帧。 */
    if ((uint32_t)(EPD_SHARE->write_seq - EPD_SHARE->read_seq) >= 2u) {
        EPD_SHARE->dropped = EPD_SHARE->dropped + 1u;
        return 0;
    }

    epd_share_fence();
    return (uint8_t *)EPD_SHARE->frame[EPD_SHARE->write_seq & 1u];
}

void epd_share_commit(void)
{
    /* 数据写完之后再发布序号 */
    epd_share_fence();
    EPD_SHARE->write_seq = EPD_SHARE->write_seq + 1u;
    IPC_ITConfig(SHARE_RDY_CH, SHARE_RDY_BIT, ENABLE);  /* 重新使能接收侧中断 */
    IPC_SetFlagStatus(SHARE_RDY_CH, SHARE_RDY_BIT);
}

void epd_share_wait_consumed(void)
{
    const uint32_t produced = EPD_SHARE->write_seq;

    /* 等到 V3F 把已经提交的帧全部取走。IPC_CH1 中断只负责把 WFI 叫醒，
     * 真正的条件用共享内存的序号判断，避免丢唤醒。 */
    while (EPD_SHARE->read_seq != produced) {
        __WFI();
    }
    epd_share_fence();
}

uint32_t epd_share_dropped(void)
{
    return EPD_SHARE->dropped;
}

const uint8_t *epd_share_peek(void)
{
    return 0;
}

void epd_share_release(void)
{
}

#elif defined(Core_V3F)

uint8_t *epd_share_begin(void)
{
    return 0;
}

void epd_share_commit(void)
{
}

uint32_t epd_share_dropped(void)
{
    return EPD_SHARE->dropped;
}

/* 消费者：有新帧就返回指向它的指针，没有返回 0 */
const uint8_t *epd_share_peek(void)
{
    const uint32_t read_seq = EPD_SHARE->read_seq;

    if ((EPD_SHARE->magic != EPD_SHARE_MAGIC) ||
        (EPD_SHARE->write_seq == read_seq)) {
        return 0;
    }

    epd_share_fence();
    return (const uint8_t *)EPD_SHARE->frame[read_seq & 1u];
}

void epd_share_release(void)
{
    epd_share_fence();
    EPD_SHARE->read_seq = EPD_SHARE->read_seq + 1u;

    /* 取走一帧后立刻允许 V5F 生产下一帧 */
    IPC_SetFlagStatus(SHARE_CMD_CH, SHARE_CMD_BIT);
}

#endif

#endif
