#include "epd_share.h"

#if defined(Core_V5F) || defined(Core_V3F)

/* 事件标志：和 printf 共用 CH0，用 bit1 表示"共享内存里有新帧" */
#define SHARE_EVT_CH     IPC_CH0
#define SHARE_EVT_BIT    IPC_CH_Sta_Bit1

static inline void epd_share_fence(void)
{
    __asm__ volatile ("fence rw, rw" ::: "memory");
}

void epd_share_init(void)
{
    // 初始化参数。V3F 先启动，通常由它清理。清理完之后暗号（magic）设置，V5F就不动了
    if (EPD_SHARE->magic != EPD_SHARE_MAGIC) {
        EPD_SHARE->write_seq = 0;
        EPD_SHARE->read_seq = 0;
        EPD_SHARE->dropped = 0;
        EPD_SHARE->prod_fps = 0;
        EPD_SHARE->cons_fps = 0;
        epd_share_fence();
        EPD_SHARE->magic = EPD_SHARE_MAGIC;
    }
    IPC_ClearFlagStatus(SHARE_EVT_CH, SHARE_EVT_BIT);
}

#if defined(Core_V5F)

uint8_t *epd_share_begin(void)
{
    /* 非阻塞：两块缓冲都还在等 V3F 取走时，这一帧直接丢掉并计数。
     * 生产者靠自己的节拍（PRODUCER_PERIOD_MS）慢下来，不在这里死等。 */
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
    IPC_SetFlagStatus(SHARE_EVT_CH, SHARE_EVT_BIT);
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

    if (EPD_SHARE->read_seq == EPD_SHARE->write_seq) {
        IPC_ClearFlagStatus(SHARE_EVT_CH, SHARE_EVT_BIT);   /* 队列空了 */
    }
}

#endif

#endif
