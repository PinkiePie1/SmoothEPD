/********************************** (C) COPYRIGHT  *******************************
 * File Name          : ch32h417_serdes.h
 * Author             : WCH
 * Version            : V1.0.1
 * Date               : 2025/10/21
 * Description        : This file contains all the functions prototypes for the
 *                      SerDes firmware library.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#ifndef __CH32H417_SERDES_H
#define __CH32H417_SERDES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ch32h417.h"

typedef enum
{
    SDSIT_EN_PhyReady = 0x1,
    SDSIT_EN_TxInt = 0x2,
    SDSIT_EN_RxErr = 0x2,
    SDSIT_EN_RxInt = 0x4,
    SDSIT_EN_FIFOOverFlow = 0x8,
    SDSIT_EN_Cominit = 0x20
} SDSIT_EN_TypeDef;

typedef enum
{
    SDSIT_FLAG_PhyReady = 0x1,
    SDSIT_FLAG_TxInt = 0x2,
    SDSIT_FLAG_RxErr = 0x2,
    SDSIT_FLAG_RxInt = 0x4,
    SDSIT_FLAG_FIFOOverFlow = 0x8,
    SDSIT_FLAG_Cominit = 0x20
} SDSIT_FLAG_TypeDef;

typedef enum
{
    SDSIT_ST_PhyReady = 0x1,
    SDSIT_ST_RxSEQMatch = 0x2,
    SDSIT_ST_RxCRCOk = 0x4,
    SDSIT_ST_PLLLock = 0x8,
    SDSIT_ST_LinkFree = 0x10,
    SDSIT_ST_RxFIFOReady = 0x20
} SDSIT_ST_TypeDef;

typedef struct
{
    uint8_t SDSRTXCtrl_LinkInit : 1;
    uint8_t SDSRTXCtrl_TxValiad : 1;
    uint8_t SDSRTXCtrl_BuffMode : 1;
    uint8_t : 5;
} SDSRTXCtrl_TypeDef;

__attribute__((aligned(4))) typedef struct
{
    uint32_t ClearALL : 1;
    uint32_t ResetLink : 1;
    uint32_t ResetPHY : 1;
    uint32_t INIBusy_En : 1;
    uint32_t SwitchRxPolarity : 1;
    uint32_t RxEn : 1;
    uint32_t TxEn : 1;
    uint32_t DMA_Enable : 1;
    uint32_t : 5;
    uint32_t : 1;
    uint32_t RxPowerUp : 1;
    uint32_t TxPowerUp : 1;
    uint32_t : 1;
    uint32_t ReplaceSYNCwithCONT : 1;
    uint32_t InsertAlign : 1;
    uint32_t : 13;
} SDS_CFG_TypeDef;

void SDS_Config(SDS_TypeDef *SDSx, SDS_CFG_TypeDef *sds_cfg);
void SDS_DMA_Tx_CFG(SDS_TypeDef *SDSx, uint32_t DMAaddr, uint32_t DataLen, uint32_t CustomWord);
ErrorStatus SDS_DMA_Rx_CFG(SDS_TypeDef *SDSx, uint32_t DMAaddr0, uint32_t DMAaddr1);
uint32_t SDS_GetFirstWord(SDS_TypeDef *SDSx, uint8_t buffnum);
void SDS_RTX_Ctrl(SDS_TypeDef *SDSx, SDSRTXCtrl_TypeDef *ctrl);
void SDS_ClearIT(SDS_TypeDef *SDSx, SDSIT_FLAG_TypeDef sdsit_fl);
SDSIT_FLAG_TypeDef SDS_ReadIT(SDS_TypeDef *SDSx);
void SDS_ConfigIT(SDS_TypeDef *SDSx, SDSIT_EN_TypeDef sdsit_en, FunctionalState en);
SDSIT_ST_TypeDef SDS_ReadCOMMAFlagBit(SDS_TypeDef *SDSx);
void SDS_GetCurrentDMA(SDS_TypeDef *SDSx, uint32_t *Addr0, uint32_t *Addr1);
void SDS_Rx_Pwrp(SDS_TypeDef *SDSx);

#ifdef __cplusplus
}
#endif

#endif
