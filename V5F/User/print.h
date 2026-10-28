#ifndef PRINT_H
#define PRINT_H

/* ============================================================================
 * PRINT() —— 所有调试输出都走这个宏
 *
 * 把 PRINT_ENABLE 改成 0，printf、格式化库、串口初始化会一起从代码里消失
 * （连 Debug/ 里的串口配置都不用跑），可以跑出最高速度。反过来想排查问题时
 * 改回 1 就行，不用动任何业务代码。
 * ==========================================================================*/

#define PRINT_ENABLE 0      // 1 = 打开调试输出, 0 = 完全屏蔽(跑最高速度)

#if PRINT_ENABLE

#include "debug.h"

#define PRINT_INIT()        USART_Printf_Init(115200)
#define PRINT(...)          printf(__VA_ARGS__)

#else

#define PRINT_INIT()      
#define PRINT(...)       

#endif

#endif
