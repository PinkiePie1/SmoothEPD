/********************************** (C) COPYRIGHT  *******************************
* File Name          : debug.h
* Author             : WCH
* Version            : V1.0.1
* Date               : 2025/09/16
* Description        : This file contains all the functions prototypes for UART
*                      Printf , Delay functions.
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#ifndef __DEBUG_H
#define __DEBUG_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stdio.h"
#include "ch32h417.h"

/* UART Printf Definition */
#define DEBUG_UART1    1
#define DEBUG_UART8    2
#define DEBUG_UART6    3
#define DEBUG_IPC      4    /* V5F：printf 走 IPC，由 V3F 从 USART1 转发 */

/* DEBUG UATR Definition */
#ifndef DEBUG

#ifdef Core_V3F
#define DEBUG   DEBUG_UART1

#elif defined(Core_V5F)
#define DEBUG   DEBUG_IPC

#endif

#endif

/* Run Core Definition */
#define Run_Core_V3F         0
#define Run_Core_V5F         1
#define Run_Core_V3FandV5F   2

#ifndef Run_Core
#define Run_Core   Run_Core_V3FandV5F
// #define Run_Core   Run_Core_V5F
// #define Run_Core   Run_Core_V3F
#endif

/* Core start addrress Definition */
#ifndef Core_V3F_StartAddr
#define Core_V3F_StartAddr   0x00000000
#endif

#ifndef Core_V5F_StartAddr
#define Core_V5F_StartAddr   0x00010000
#endif

#if ((Run_Core == Run_Core_V3FandV5F) || (Run_Core == Run_Core_V5F)) && defined(Core_V5F) 
#define Func_Run_V5F 1
#define Func_Run_V3F 0
#elif ((Run_Core == Run_Core_V3FandV5F) || (Run_Core == Run_Core_V3F)) && defined(Core_V3F)
#define Func_Run_V5F 0
#define Func_Run_V3F 1
#else
#define Func_Run_V5F 0
#define Func_Run_V3F 0
#endif

extern volatile uint32_t WFE_WkupSource;

void Delay_Init(void);
void Delay_Us (uint32_t n);
void Delay_Ms (uint32_t n);
void USART_Printf_Init(uint32_t baudrate);

#ifdef __cplusplus
}
#endif

#endif 



