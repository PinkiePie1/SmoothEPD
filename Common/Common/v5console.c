#include "v5console.h"

/* 只在这一对核上编译这个文件的内容（两边都在各自工程里编同一份） */
#if defined(Core_V5F) || defined(Core_V3F)

/* IPC 资源分配：
 *   CH0          —— 专用一条通道
 *   MSG0         —— 传一个字节（低 8 位有效）
 *   STS bit0     —— 1 = V5F 放下了新字节 / 0 = V3F 已经取走
 */
#define V5CON_CH     IPC_CH0
#define V5CON_MSG    IPC_MSG0
#define V5CON_FLAG   IPC_CH_Sta_Bit0

void v5con_init(void)
{
    IPC_InitTypeDef ipc = {0};

    IPC_StructInit(&ipc);
    ipc.IPC_CH = V5CON_CH;
    ipc.TxCID = IPC_TxCID0;
    ipc.RxCID = IPC_RxCID0;
    ipc.TxIER = DISABLE;
    ipc.RxIER = DISABLE;
    ipc.AutoEN = DISABLE;
    IPC_Init(&ipc);

    /* 上电时把标志清干净，避免第一次 putc 就被当成"已取走" */
    IPC_ClearFlagStatus(V5CON_CH, V5CON_FLAG);
}

#if defined(Core_V5F)

void v5con_putc(char c)
{
    uint32_t guard = 4000000u;

    IPC_WriteMSG(V5CON_MSG, (uint32_t)(uint8_t)c);
    IPC_SetFlagStatus(V5CON_CH, V5CON_FLAG);

    /* 等 V3F 取走。V3F 没在跑（比如只烧了 V5F）时不能死等，超时就丢掉。 */
    while (IPC_GetFlagStatus(V5CON_CH, V5CON_FLAG) != RESET) {
        if (--guard == 0u) {
            IPC_ClearFlagStatus(V5CON_CH, V5CON_FLAG);
            return;
        }
    }
}

#elif defined(Core_V3F)

void v5con_poll(void)
{
    while (IPC_GetFlagStatus(V5CON_CH, V5CON_FLAG) != RESET) {
        const char c = (char)(uint8_t)IPC_ReadMSG(V5CON_MSG);

        IPC_ClearFlagStatus(V5CON_CH, V5CON_FLAG);

        USART_SendData(USART1, (uint8_t)c);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
        }
    }
}

#endif

#endif
