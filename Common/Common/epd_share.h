#ifndef EPD_SHARE_H
#define EPD_SHARE_H

/* ============================================================================
 * 双核显示流水线的共享内存协议
 *
 *   V5F（内容生成）        ── A/B 双缓冲 ──>        V3F（显示运算 + 驱动面板）
 *   画好一帧写进空槽                            按 FIFO 取帧：脉冲 budget/掩码/SPI
 *   write_seq++，用信号量通知                     read_seq++，槽位还给 V5F
 *
 * 共享块放在 V3F 的 RAM（0x20170000），实测 V5F 能读能写同一块物理内存。
 *
 * 同步机制：
 *   - write_seq / read_seq 是"计数信号量"：write_seq - read_seq = 待消费帧数，
 *     上限 2（正好两块缓冲）。满了 V5F 就丢帧（dropped 计数），绝不覆盖未消费的帧。
 *   - 另外用 IPC CH0 的 bit1 当"有新帧"的事件标志，方便以后改成中断唤醒。
 * ==========================================================================*/

#include "ch32h417.h"
#include "epd_fast.h"

#define EPD_SHARE_ADDR   0x20170000u          /* 放在 V3F 的 RAM 里，两核共享 */
#define EPD_SHARE_MAGIC  0x45504432u          /* 'EPD2' */

//#define EPD_W            152
//#define EPD_H            152
//#define EPD_STRIDE       19                   /* 每行 152 像素 = 19 字节 */
//#define EPD_FRAME_BYTES  (19 * EPD_H) /* 2888 */

typedef struct
{
    volatile uint32_t magic;          /* 初始化完成后写成 EPD_SHARE_MAGIC */
    volatile uint32_t write_seq;      /* V5F：每写成一帧 +1 */
    volatile uint32_t read_seq;       /* V3F：每取走一帧 +1 */
    volatile uint32_t dropped;        /* V5F：缓冲满而丢掉的帧数 */
    volatile uint32_t prod_fps;       /* V5F 自报帧率，单位 0.1 fps */
    volatile uint32_t cons_fps;       /* V3F 自报帧率，单位 0.1 fps */
    uint8_t frame[2][EPD_FRAME_BYTES];/* A/B 两块位图 */
} epd_share_t;

#define EPD_SHARE  ((volatile epd_share_t *)EPD_SHARE_ADDR)

/* 生产者（V5F）：拿一块可以写的缓冲；返回 NULL 表示上一帧还没被取走。
 * 拿到之后往里面画，画完调 epd_share_commit()。 */
uint8_t *epd_share_begin(void);

/* 生产者（V5F）：提交这一帧（write_seq++ 并置事件标志）。 */
void epd_share_commit(void);

/* 生产者统计：因为 V3F 没来得及取而丢掉的帧数。 */
uint32_t epd_share_dropped(void);

/* 消费者（V3F）：按 FIFO 顺序取最旧未消费帧；返回 NULL 表示没有新帧。 */
const uint8_t *epd_share_peek(void);

/* 消费者（V3F）：这一帧用完了，把槽位还回去（read_seq++）。 */
void epd_share_release(void);

/* 两核都要在初始化时调一次（幂等，只清一次）。 */
void epd_share_init(void);

#endif
