#ifndef V5CONSOLE_H
#define V5CONSOLE_H

/* ============================================================================
 * 跨核控制台：把 V5F 的 printf 通过 IPC 送到 V3F，由 V3F 从 USART1 打出来
 *
 * V5F 侧：_write 每字符调用 v5con_putc()，带握手（V3F 取走后才返回），
 *         这样根本不需要缓冲，printf 天然就被限速了。
 * V3F 侧：主循环里不停调 v5con_poll()，把排队的字节打到 USART1。
 *
 * 用 IPC 通道 CH0：MSG0 传一个字节，STS 的 bit0 当"有新字节"标志。
 * ==========================================================================*/

#include "ch32h417.h"

void v5con_init(void);

#if defined(Core_V5F)
void v5con_putc(char c);
#elif defined(Core_V3F)
void v5con_poll(void);
#endif

#endif
