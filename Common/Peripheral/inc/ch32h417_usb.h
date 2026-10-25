/********************************** (C) COPYRIGHT  *******************************
* File Name          : ch32h417_usb.h
* Author             : WCH
* Version            : V1.0.2
* Date               : 2026/04/10
* Description        : This file contains all the functions prototypes for the 
*                      USB firmware library.
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef __CH32H417_USB_H
#define __CH32H417_USB_H

#ifdef __cplusplus
 extern "C" {
#endif

/*******************************************************************************/
/* Header File */
#include "stdint.h"

/*******************************************************************************/
/* USB Communication Related Macro Definition */
/* USB Endpoint0 Size */
#ifndef DEFAULT_ENDP0_SIZE
#define DEFAULT_ENDP0_SIZE          8          // default maximum packet size for endpoint 0
#endif
#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE             64         // maximum packet size
#endif

/* USB PID */
#ifndef USB_PID_SETUP
#define USB_PID_NULL                0x00
#define USB_PID_SOF                 0x05
#define USB_PID_SETUP               0x0D
#define USB_PID_IN                  0x09
#define USB_PID_OUT                 0x01
#define USB_PID_NYET                0x06
#define USB_PID_ACK                 0x02
#define USB_PID_NAK                 0x0A
#define USB_PID_STALL               0x0E
#define USB_PID_DATA0               0x03
#define USB_PID_DATA1               0x0B
#define USB_PID_DATA2               0x07
#define USB_PID_MDATA               0x0F
#define USB_PID_PRE                 0x0C
#endif

/* USB standard device request code */
#ifndef USB_GET_DESCRIPTOR
#define USB_GET_STATUS              0x00
#define USB_CLEAR_FEATURE           0x01
#define USB_SET_ENDPOINT            0x02
#define USB_SET_FEATURE             0x03
#define USB_SET_ADDRESS             0x05
#define USB_GET_DESCRIPTOR          0x06
#define USB_SET_DESCRIPTOR          0x07
#define USB_GET_CONFIGURATION       0x08
#define USB_SET_CONFIGURATION       0x09
#define USB_GET_INTERFACE           0x0A
#define USB_SET_INTERFACE           0x0B
#define USB_SYNCH_FRAME             0x0C
#define USB_SET_SEL                 0x30
#define USB_SET_ISOCH_DLY           0x31
#endif

#define DEF_STRING_DESC_LANG        0x00
#define DEF_STRING_DESC_MANU        0x01
#define DEF_STRING_DESC_PROD        0x02
#define DEF_STRING_DESC_SERN        0x03
#define DEF_STRING_DESC_OS          0xEE

/* USB hub class request code */
#ifndef HUB_GET_DESCRIPTOR
#define HUB_GET_STATUS              0x00
#define HUB_CLEAR_FEATURE           0x01
#define HUB_GET_STATE               0x02
#define HUB_SET_FEATURE             0x03
#define HUB_GET_DESCRIPTOR          0x06
#define HUB_SET_DESCRIPTOR          0x07
#endif

/* USB HID class request code */
#ifndef HID_GET_REPORT
#define HID_GET_REPORT              0x01
#define HID_GET_IDLE                0x02
#define HID_GET_PROTOCOL            0x03
#define HID_SET_REPORT              0x09
#define HID_SET_IDLE                0x0A
#define HID_SET_PROTOCOL            0x0B
#endif

/* USB CDC Class request code */
#ifndef CDC_GET_LINE_CODING
#define CDC_GET_LINE_CODING         0x21                                      /* This request allows the host to find out the currently configured line coding */
#define CDC_SET_LINE_CODING         0x20                                      /* Configures DTE rate, stop-bits, parity, and number-of-character */
#define CDC_SET_LINE_CTLSTE         0x22                                      /* This request generates RS-232/V.24 style control signals */
#define CDC_SEND_BREAK              0x23                                      /* Sends special carrier modulation used to specify RS-232 style break */
#endif

/* USB UVC device request code */
#define UVC_GET_CUR                 0x81 
#define UVC_GET_MIN                 0x82 
#define UVC_GET_MAX                 0x83 
#define UVC_GET_RES                 0x84 
#define UVC_GET_LEN                 0x85 
#define UVC_GET_INFO                0x86 
#define UVC_GET_DEF                 0x87 

/* Bit Define for USB Request Type */
#ifndef USB_REQ_TYP_MASK
#define USB_REQ_TYP_IN              0x80
#define USB_REQ_TYP_OUT             0x00
#define USB_REQ_TYP_READ            0x80
#define USB_REQ_TYP_WRITE           0x00
#define USB_REQ_TYP_MASK            0x60
#define USB_REQ_TYP_STANDARD        0x00
#define USB_REQ_TYP_CLASS           0x20
#define USB_REQ_TYP_VENDOR          0x40
#define USB_REQ_TYP_RESERVED        0x60
#define USB_REQ_RECIP_MASK          0x1F
#define USB_REQ_RECIP_DEVICE        0x00
#define USB_REQ_RECIP_INTERF        0x01
#define USB_REQ_RECIP_ENDP          0x02
#define USB_REQ_RECIP_OTHER         0x03
#define USB_REQ_FEAT_REMOTE_WAKEUP  0x01
#define USB_REQ_FEAT_ENDP_HALT      0x00
#endif

/* USB Descriptor Type */
#ifndef USB_DESCR_TYP_DEVICE
#define USB_DESCR_TYP_DEVICE        0x01
#define USB_DESCR_TYP_CONFIG        0x02
#define USB_DESCR_TYP_STRING        0x03
#define USB_DESCR_TYP_INTERF        0x04
#define USB_DESCR_TYP_ENDP          0x05
#define USB_DESCR_TYP_QUALIF        0x06
#define USB_DESCR_TYP_SPEED         0x07
#define USB_DESCR_TYP_OTG           0x09
#define USB_DESCR_TYP_BOS           0X0F
#define USB_DESCR_TYP_HID           0x21
#define USB_DESCR_TYP_REPORT        0x22
#define USB_DESCR_TYP_PHYSIC        0x23
#define USB_DESCR_TYP_CS_INTF       0x24
#define USB_DESCR_TYP_CS_ENDP       0x25
#define USB_DESCR_TYP_HUB           0x29
#endif

/* USB Device Class */
#ifndef USB_DEV_CLASS_HUB
#define USB_DEV_CLASS_RESERVED      0x00
#define USB_DEV_CLASS_AUDIO         0x01
#define USB_DEV_CLASS_COMMUNIC      0x02
#define USB_DEV_CLASS_HID           0x03
#define USB_DEV_CLASS_MONITOR       0x04
#define USB_DEV_CLASS_PHYSIC_IF     0x05
#define USB_DEV_CLASS_POWER         0x06
#define USB_DEV_CLASS_IMAGE         0x06
#define USB_DEV_CLASS_PRINTER       0x07
#define USB_DEV_CLASS_STORAGE       0x08
#define USB_DEV_CLASS_HUB           0x09
#define USB_DEV_CLASS_VEN_SPEC      0xFF
#endif

/* USB Hub Class Request */
#ifndef HUB_GET_HUB_DESCRIPTOR
#define HUB_CLEAR_HUB_FEATURE       0x20
#define HUB_CLEAR_PORT_FEATURE      0x23
#define HUB_GET_BUS_STATE           0xA3
#define HUB_GET_HUB_DESCRIPTOR      0xA0
#define HUB_GET_HUB_STATUS          0xA0
#define HUB_GET_PORT_STATUS         0xA3
#define HUB_SET_HUB_DESCRIPTOR      0x20
#define HUB_SET_HUB_FEATURE         0x20
#define HUB_SET_PORT_FEATURE        0x23
#endif

/* Hub Class Feature Selectors */
#ifndef HUB_PORT_RESET
#define HUB_C_HUB_LOCAL_POWER       0
#define HUB_C_HUB_OVER_CURRENT      1
#define HUB_PORT_CONNECTION         0
#define HUB_PORT_ENABLE             1
#define HUB_PORT_SUSPEND            2
#define HUB_PORT_OVER_CURRENT       3
#define HUB_PORT_RESET              4
#define HUB_PORT_POWER              8
#define HUB_PORT_LOW_SPEED          9
#define HUB_C_PORT_CONNECTION       16
#define HUB_C_PORT_ENABLE           17
#define HUB_C_PORT_SUSPEND          18
#define HUB_C_PORT_OVER_CURRENT     19
#define HUB_C_PORT_RESET            20
#endif


/* USB UDisk */
#ifndef USB_BO_CBW_SIZE
#define USB_BO_CBW_SIZE             0x1F
#define USB_BO_CSW_SIZE             0x0D
#endif
#ifndef USB_BO_CBW_SIG0
#define USB_BO_CBW_SIG0             0x55
#define USB_BO_CBW_SIG1             0x53
#define USB_BO_CBW_SIG2             0x42
#define USB_BO_CBW_SIG3             0x43
#define USB_BO_CSW_SIG0             0x55
#define USB_BO_CSW_SIG1             0x53
#define USB_BO_CSW_SIG2             0x42
#define USB_BO_CSW_SIG3             0x53
#endif

#define USB_U1_ENABLE               0x30
#define USB_U2_ENABLE               0x31
/*******************************************************************************/
/* USBSS Related Register Macro Definition */

/* LINK_CFG */
#define U3_LINK_RESET               0x80000000
#define LINK_FORCE_RXTERM           0x00800000
#define LINK_FORCE_POLLING          0x00400000
#define LINK_TOUT_MODE              0x00200000
#define LINK_U1_PING_EN             0x00100000
#define LINK_U2_ALLOW               0x00020000
#define LINK_U1_ALLOW               0x00010000
#define LINK_LTSSM_MODE             0x00008000
#define LINK_LOOPBACK_ACT           0x00004000
#define LINK_LOOPBACK_EN            0x00002000
#define LINK_U2_RXDET               0x00001000
#define LINK_CP78_SEL_MASK          0x00000C00
 #define LINK_CP78_SEL_190BITS      0x00000000
 #define LINK_CP78_SEL_120BITS      0x00000400
 #define LINK_CP78_SEL_50BITS       0x00000800
 #define LINK_CP78_SEL_250BITS      0x00000C00
#define LINK_TX_DEEMPH_MASK         0x00000300
 #define LINK_TX_DEEMPH_6DB         0x00000000
 #define LINK_TX_DEEMPH_3_5DB       0x00000100
#define LINK_TX_SWING               0x00000080
#define LINK_RX_EQ_EN               0x00000040
#define LINK_LFPS_RX_PD             0x00000020
#define LINK_COMPLIANCE_EN          0x00000010
#define LINK_PHY_RESET              0x00000008
#define LINK_SS_PLR_SWAP            0x00000004
#define LINK_RX_TERM_EN             0x00000002
#define LINK_DOWN_MODE              0x00000001

/* LINK_CTRL */
#define LINK_RX_TS_CFG_MASK         0xFF000000
#define LINK_TX_TS_CFG_MASK         0x00FF0000
 #define LINK_HOT_RESET             0x00010000
#define LINK_TX_LGO_U3              0x00008000
#define LINK_TX_LGO_U2              0x00004000
#define LINK_TX_LGO_U1              0x00002000
#define LINK_POLLING_EN             0x00001000
#define LINK_REG_ROUT_EN            0x00000800
#define LINK_LUP_LDN_EN             0x00000400
#define LINK_TX_UX_EXIT             0x00000200
#define LINK_TX_WARM_RESET          0x00000100
#define LINK_GO_RX_DET              0x00000080
#define LINK_GO_RECOVERY            0x00000040
#define LINK_GO_INACTIVE            0x00000020
#define LINK_GO_DISABLED            0x00000010
 #define LINK_PD_MODE_MASK           0x00000003
#define LINK_P0_MODE                0x00000000
#define LINK_P1_MODE                0x00000001
#define LINK_P2_MODE                0x00000002
#define LINK_P3_MODE                0x00000003

/* LINK_INT_CTRL */
#define LINK_IE_STATE_CHG           0x80000000
#define LINK_IE_U1_TOUT             0x40000000
#define LINK_IE_U2_TOUT             0x20000000
#define LINK_IE_UX_FAIL             0x10000000
#define LINK_IE_TX_WARMRST          0x08000000
#define LINK_IE_UX_EXIT_FAIL        0x04000000
#define LINK_IE_RX_LMP_TOUT         0x00800000
#define LINK_IE_TX_LMP              0x00400000
#define LINK_IE_RX_LMP              0x00200000
#define LINK_IE_RX_DET              0x00100000
#define LINK_IE_LOOPBACK            0x00080000
#define LINK_IE_COMPLIANCE          0x00040000
#define LINK_IE_RX_SET_FC           0x00020000
#define LINK_IE_HPBUF_EMPTY         0x00010000
#define LINK_IE_HOT_RST             0x00008000
#define LINK_IE_WAKEUP              0x00004000
#define LINK_IE_WARM_RST            0x00002000
#define LINK_IE_UX_EXIT             0x00001000
#define LINK_IE_TXEQ                0x00000800
#define LINK_IE_TERM_PRES           0x00000400
#define LINK_IE_UX_REJ              0x00000200
#define LINK_IE_U3_WK_TOUT          0x00000100
#define LINK_IE_GO_U0               0x00000080
#define LINK_IE_GO_U1               0x00000040
#define LINK_IE_GO_U2               0x00000020
#define LINK_IE_GO_U3               0x00000010
#define LINK_IE_DISABLE             0x00000008
#define LINK_IE_INACTIVE            0x00000004
#define LINK_IE_RECOVERY            0x00000002
#define LINK_IE_READY               0x00000001

/* LINK_INT_FLAG */
#define LINK_IF_STATE_CHG           0x80000000
#define LINK_IF_U1_TOUT             0x40000000
#define LINK_IF_U2_TOUT             0x20000000
#define LINK_IF_UX_FAIL             0x10000000
#define LINK_IF_TX_WARMRST          0x08000000
#define LINK_IF_UX_EXIT_FAIL        0x04000000
#define LINK_IF_RX_LMP_TOUT         0x00800000
#define LINK_IF_TX_LMP              0x00400000
#define LINK_IF_RX_LMP              0x00200000
#define LINK_IF_RX_DET              0x00100000
#define LINK_IF_LOOPBACK            0x00080000
#define LINK_IF_COMPLIANCE          0x00040000
#define LINK_IF_RX_SET_FC           0x00020000
#define LINK_IF_HPBUF_EMPTY         0x00010000
#define LINK_IF_HOT_RST             0x00008000
#define LINK_IF_WAKEUP              0x00004000
#define LINK_IF_WARM_RST            0x00002000
#define LINK_IF_UX_EXIT             0x00001000
#define LINK_IF_TXEQ                0x00000800
#define LINK_IF_TERM_PRES           0x00000400
#define LINK_IF_UX_REJ              0x00000200
#define LINK_IF_U3_WK_TOUT          0x00000100
#define LINK_IF_GO_U0               0x00000080
#define LINK_IF_GO_U1               0x00000040
#define LINK_IF_GO_U2               0x00000020
#define LINK_IF_GO_U3               0x00000010
#define LINK_IF_DISABLE             0x00000008
#define LINK_IF_INACTIVE            0x00000004
#define LINK_IF_RECOVERY            0x00000002
#define LINK_IF_READY               0x00000001

/* LINK_STATUS */
#define LINK_HPBUF_EMPTY            0x80000000
#define LINK_HPBUF_FULL             0x40000000
#define LINK_HPBUF_IDLE             0x20000000
#define LINK_U3_SLEEP_ALLOW         0x00400000
#define LINK_U2_SLEEP_ALLOW         0x00200000
#define LINK_RXDET_SLEEP_ALLOW      0x00100000
#define LINK_WAKUP                  0x00080000
#define LINK_RX_LFPS                0x00040000
#define LINK_RX_DETECT              0x00020000
#define LINK_RX_UX_EXIT_REQ         0x00010000
#define LINK_STATE_MASK             0x00000F00
 #define LINK_STATE_U0              0x00000000
 #define LINK_STATE_U1              0x00000100
 #define LINK_STATE_U2              0x00000200
 #define LINK_STATE_U3              0x00000300
 #define LINK_STATE_DISABLE         0x00000400
 #define LINK_STATE_RXDET           0x00000500
 #define LINK_STATE_INACTIVE        0x00000600
 #define LINK_STATE_POLLING         0x00000700
 #define LINK_STATE_RECOVERY        0x00000800
 #define LINK_STATE_HOTRST          0x00000900
 #define LINK_STATE_COMPLIANCE      0x00000A00
 #define LINK_STATE_LOOPBACK        0x00000B00
#define LINK_TXEQ                   0x00000040
#define LINK_PD_MODE_ST_MASK        0x00000030
 #define LINK_PD_MODE_ST_P0         0x00000000
 #define LINK_PD_MODE_ST_P1         0x00000010
 #define LINK_PD_MODE_ST_P2         0x00000020
 #define LINK_PD_MODE_ST_P3         0x00000030
#define LINK_READY                  0x00000008
#define LINK_BUSY                   0x00000004
#define LINK_RX_WARM_RST            0x00000002
#define LINK_RX_TERM_PRES           0x00000001

/* LINK_LPM_CR */
#define LINK_LPM_TERM_PRESENT       0x00000800
#define LINK_LPM_TERM_CHG           0x00000400
#define LINK_LPM_EN                 0x00000200
#define LINK_LPM_RST                0x00000100

/* LINK_PORT_CAP */
#define LINK_LMP_RX_CAP_VLD         0x80000000
#define LINK_LMP_TX_CAP_VLD         0x40000000
#define FORCE_PM                    0x20000000 
#define LINK_SPEED_MASK             0x3F000000
#define LINK_PORT_CAP_MASK          0x00FFFFFF

/* USB_CONTROL */
#define USBSS_DEV_ADDR_MASK         0x7F000000
#define USBSS_UIE_FIFO_RXOV         0x00800000           
#define USBSS_UIE_FIFO_TXOV         0x00400000           
#define USBSS_UIE_ITP               0x00100000       
#define USBSS_UIE_RX_PING           0x00080000         
#define USBSS_UDIE_STATUS           0x00040000
#define USBSS_UHIE_NOTIF            0x00040000  
#define USBSS_UDIE_SETUP            0x00020000       
#define USBSS_UHIE_ERDY             0x00020000     
#define USBSS_UIE_TRANSFER          0x00010000           
#define USBSS_CHAIN_CONFLICT        0x00008000               
#define USBSS_TX_ERDY_MODE          0x00004000           
#define USBSS_HP_PEND_MASK          0x00000300    
 #define USBSS_HP_PENDING           0x00000200
#define USBSS_HOST_MODE             0x00000080       
#define USBSS_ITP_EN                0x00000040       
#define USBSS_SETUP_FLOW            0x00000020           
#define USBSS_DIR_ABORT             0x00000010       
#define USBSS_DMA_MODE              0x00000008       
#define USBSS_FORCE_RST             0x00000004       
#define USBSS_USB_CLR_ALL           0x00000002           
#define USBSS_DMA_EN                0x00000001  

/* USB_STATUS */
#define USBSS_HRX_RES_MASK          0xC0000000
#define USBSS_HTX_RES_MASK          0x00C00000
#define USBSS_EP_DIR_MASK           0x00001000
#define USBSS_EP_ID_MASK            0x00000700
#define USBSS_UIF_FIFO_RXOV         0x00000080  
#define USBSS_UIF_FIFO_TXOV         0x00000040           
#define USBSS_UIF_ITP               0x00000010       
#define USBSS_UIF_RX_PING           0x00000008           
#define USBSS_UDIF_STATUS           0x00000004           
#define USBSS_UHIF_NOTIF            0x00000004           
#define USBSS_UDIF_SETUP            0x00000002           
#define USBSS_UHIF_ERDY             0x00000002       
#define USBSS_UIF_TRANSFER          0x00000001   

/* USB_ITP */
#define USBSS_ITP_INTERVAL_MASK     0x00003FFF

/* USB_ITP_ADJ */
#define USBSS_ITP_DELTA             0x001FFF00
#define USBSS_ITP_DELAYED           0x00000080
#define USBSS_ITP_ADJ_CR_MASK       0x0000007F

/* UEP_TX_EN */
#define USBSS_EP15_TX_EN            0x00008000
#define USBSS_EP14_TX_EN            0x00004000
#define USBSS_EP13_TX_EN            0x00002000
#define USBSS_EP12_TX_EN            0x00001000
#define USBSS_EP11_TX_EN            0x00000800
#define USBSS_EP10_TX_EN            0x00000400
#define USBSS_EP9_TX_EN             0x00000200
#define USBSS_EP8_TX_EN             0x00000100
#define USBSS_EP7_TX_EN             0x00000080
#define USBSS_EP6_TX_EN             0x00000040
#define USBSS_EP5_TX_EN             0x00000020
#define USBSS_EP4_TX_EN             0x00000010
#define USBSS_EP3_TX_EN             0x00000008
#define USBSS_EP2_TX_EN             0x00000004
#define USBSS_EP1_TX_EN             0x00000002
#define USBSS_UH_TX_EN              0x00000002

/* UEP_RX_EN */
#define USBSS_EP15_RX_EN            0x00008000
#define USBSS_EP14_RX_EN            0x00004000
#define USBSS_EP13_RX_EN            0x00002000
#define USBSS_EP12_RX_EN            0x00001000
#define USBSS_EP11_RX_EN            0x00000800
#define USBSS_EP10_RX_EN            0x00000400
#define USBSS_EP9_RX_EN             0x00000200
#define USBSS_EP8_RX_EN             0x00000100
#define USBSS_EP7_RX_EN             0x00000080
#define USBSS_EP6_RX_EN             0x00000040
#define USBSS_EP5_RX_EN             0x00000020
#define USBSS_EP4_RX_EN             0x00000010
#define USBSS_EP3_RX_EN             0x00000008
#define USBSS_EP2_RX_EN             0x00000004
#define USBSS_EP1_RX_EN             0x00000002
#define USBSS_UH_RX_EN              0x00000002

/* UEP0_TX_CTRL */
#define USBSS_UIF_EP0_TX_ACT        0x80000000
#define USBSS_EP0_TX_FLOW           0x02000000
#define USBSS_EP0_TX_PP             0x01000000
#define USBSS_EP0_TX_ERDY           0x00800000
#define USBSS_EP0_TX_RES            0x00600000
 #define USBSS_EP0_TX_NRDY          0x00000000
 #define USBSS_EP0_TX_DPH           0x00200000
 #define USBSS_EP0_TX_STALL         0x00400000
#define USBSS_EP0_TX_SEQ_MASK       0x001F0000 
 #define USBSS_NUMP_1               0x00010000 
 #define USBSS_NUMP_2               0x00020000 
 #define USBSS_NUMP_3               0x00030000 
 #define USBSS_NUMP_4               0x00040000 
 #define USBSS_NUMP_5               0x00050000 
 #define USBSS_NUMP_6               0x00060000 
 #define USBSS_NUMP_7               0x00070000 
 #define USBSS_NUMP_8               0x00080000 
 #define USBSS_NUMP_9               0x00090000 
 #define USBSS_NUMP_10              0x000A0000 
 #define USBSS_NUMP_11              0x000B0000
 #define USBSS_NUMP_12              0x000C0000
 #define USBSS_NUMP_13              0x000D0000
 #define USBSS_NUMP_14              0x000E0000
 #define USBSS_NUMP_15              0x000F0000
 #define USBSS_NUMP_16              0x00100000 
#define USBSS_EP0_TX_LEN_MASK       0x000007FF

/* UEP0_RX_CTRL */
#define USBSS_UIF_EP0_RX_ACT        0x80000000
#define USBSS_EP0_RX_PP             0x01000000
#define USBSS_EP0_RX_ERDY           0x00800000
#define USBSS_EP0_RX_RES            0x00600000
 #define USBSS_EP0_RX_NRDY          0x00000000
 #define USBSS_EP0_RX_ACK           0x00200000
 #define USBSS_EP0_RX_STALL         0x00400000
#define USBSS_EP0_RX_SEQ_MASK       0x001F0000
#define USBSS_EP0_RX_LEN_MASK       0x000007FF

/* R8_UEPn_TX_CFG */
#define USBSS_EP_TX_CHAIN_AUTO      0x80
#define USBSS_EP_TX_FIFO_MODE       0x40
#define USBSS_EP_TX_FIFO_CFG        0x20
#define USBSS_EP_TX_EOB_MODE        0x08
#define USBSS_EP_TX_ERDY_AUTO       0x04
#define USBSS_EP_TX_SEQ_AUTO        0x02
#define USBSS_EP_TX_ISO_MODE        0x01

/* R8_UEPn_TX_CR */
#define USBSS_EP_TX_HALT            0x80
#define USBSS_EP_TX_CLR             0x40
#define USBSS_EP_TX_CHAIN_CLR       0x20
#define USBSS_EP_TX_ERDY_NUMP_MASK  0x1F


/* R8_UEPn_TX_SEQ */
#define USBSS_EP_TX_SEQ_NUM_MASK    0x1F

/* R8_UEPn_TX_ST */
#define USBSS_EP_TX_INT_FLAG        0x80
#define USBSS_EP_TX_FC_ST           0x40
#define USBSS_EP_TX_ERDY_REQ        0x20
#define USBSS_EP_TX_CHAIN_RES       0x10
#define USBSS_EP_TX_CHAIN_EN_MASK   0x0F

/* R8_UEPn_TX_CHAIN_CR */
#define USBSS_EP_TX_CUR_USE         0xC0               
#define USBSS_EP_TX_CUR_CFG         0x30               
#define USBSS_EP_TX_FORCE_RET       0x04             
#define USBSS_EP_TX_RET_SEL         0x03               

/* R8_UEPn_TX_CHAIN_ST */
#define USBSS_EP_TX_CHAIN_EN        0x80
#define USBSS_EP_TX_CHAIN_IF        0x40
#define USBSS_EP_TX_EOB_LPF         0x20
#define USBSS_EP_TX_NUMP_EMPTY      0x08
#define USBSS_EP_TX_DPH_PP          0x04
#define USBSS_EP_TX_CHAIN_NO_MASK   0x03

/* R8_UEPn_RX_CFG */
#define USBSS_EP_RX_CHAIN_AUTO      0x80
#define USBSS_EP_RX_FIFO_MODE       0x40
#define USBSS_EP_RX_FIFO_CFG        0x20
#define USBSS_EP_RX_EOB_MODE        0x08
#define USBSS_EP_RX_ERDY_AUTO       0x04
#define USBSS_EP_RX_SEQ_AUTO        0x02
#define USBSS_EP_RX_ISO_MODE        0x01

/* R8_UEPn_RX_CR */
#define USBSS_EP_RX_HALT            0x80
#define USBSS_EP_RX_CLR             0x40
#define USBSS_EP_RX_CHAIN_CLR       0x20
#define USBSS_EP_RX_ERDY_NUMP_MASK  0x1F

/* R8_UEPn_RX_SEQ */
#define USBSS_EP_RX_SEQ_NUM_MASK    0x1F

/* R8_UEPn_RX_ST */
#define USBSS_EP_RX_INT_FLAG        0x80
#define USBSS_EP_RX_FC_ST           0x40
#define USBSS_EP_RX_ERDY_REQ        0x20
#define USBSS_EP_RX_CHAIN_RES       0x10
#define USBSS_EP_RX_CHAIN_EN_MASK   0x0F

/* R8_UEPn_RX_CHAIN_CR */
#define USBSS_EP_RX_CUR_USE         0xC0               
#define USBSS_EP_RX_CUR_CFG         0x30               
#define USBSS_EP_RX_FORCE_RET       0x04             
#define USBSS_EP_RX_RET_SEL         0x03               

/* R8_UEPn_RX_CHAIN_ST */
#define USBSS_EP_RX_CHAIN_EN        0x80
#define USBSS_EP_RX_CHAIN_IF        0x40
#define USBSS_EP_RX_LPF_FLAG        0x20
#define USBSS_EP_RX_ISO_PKT_ERR     0x10
#define USBSS_EP_RX_NUMP_EMPTY      0x08
#define USBSS_EP_RX_DPH_PP          0x04
#define USBSS_EP_RX_CHAIN_NO_MASK   0x03

/* R32_UH_TX_CTRL */
#define USBSS_UH_TX_ACT             0x80000000
#define USBSS_UH_TX_ISO             0x40000000
#define USBSS_UH_TX_SETUP           0x20000000
#define USBSS_UH_TX_STATUS          0x10000000
#define USBSS_UH_TX_LPF             0x00800000
#define USBSS_UH_TX_RES             0x00600000
 #define USBSS_UH_TX_NRDY           0x00000000
 #define USBSS_UH_TX_ACK            0x00200000
 #define USBSS_UH_TX_STALL          0x00400000
#define USBSS_UH_TX_SEQ             0x001F0000
#define USBSS_UH_TX_EP              0x0000F000
#define USBSS_UH_TX_LEN_MASK        0x000007FF  

/* R32_UH_RX_CTRL */
#define USBSS_UH_RX_ACT             0x80000000
#define USBSS_UH_RX_ISO             0x40000000
#define USBSS_UH_RX_NUMP            0x1F000000
#define USBSS_UH_RX_RES             0x00600000
 #define USBSS_UH_RX_NRDY           0x00000000
 #define USBSS_UH_RX_ACK            0x00200000
 #define USBSS_UH_RX_STALL          0x00400000
#define USBSS_UH_RX_SEQ             0x001F0000
#define USBSS_UH_RX_EP              0x0000F000
#define USBSS_UH_RX_LEN_MASK        0x000007FF   

/* R32_HOST_STATUS */
#define USBSS_UH_ITP_PRESAGE        0x000C0000
#define USBSS_UH_RX_ISO_PKT_ERR     0x00020000
#define USBSS_UH_RX_EOB_LPF         0x00010000
#define USBSS_UH_RX_ERDY_DIR        0x00008000
#define USBSS_UH_RX_ERDY_NUMP       0x00001F00
#define USBSS_UH_RX_ERDY_EP         0x000000F0

/*******************************************************************************/
/* USBHS Related Register Macro Definition */

/* R8_USB_CTRL */
#define  USBHS_UD_LPM_EN            0x80
#define  USBHS_UD_DEV_EN            0x20
#define  USBHS_UD_DMA_EN            0x10
#define  USBHS_UD_PHY_SUSPENDM      0x08
#define  USBHS_UD_CLR_ALL           0x04
#define  USBHS_UD_RST_SIE           0x02
#define  USBHS_UD_RST_LINK          0x01

/* R8_USB_BASE_MODE */
#define  USBHS_UD_SPEED_FULL        0x00
#define  USBHS_UD_SPEED_HIGH        0x01
#define  USBHS_UD_SPEED_LOW         0x02
#define  USBHS_UD_SPEED_TYPE        0x03       

/* R8_USB_INT_EN */
#define  USBHS_UDIE_FIFO_OVER       0x80
#define  USBHS_UDIE_LINK_RDY        0x40
#define  USBHS_UDIE_SOF_ACT         0x20
#define  USBHS_UDIE_TRANSFER        0x10
#define  USBHS_UDIE_LPM_ACT         0x08
#define  USBHS_UDIE_BUS_SLEEP       0x04
#define  USBHS_UDIE_SUSPEND         0x02
#define  USBHS_UDIE_BUS_RST         0x01

/* R8_USB_DEV_AD */
#define  USBHS_UD_DEV_ADDR          0x7F

/* R8_USB_WAKE_CTRL */
#define  USBHS_UD_REMOTE_WKUP       0x01

/* R8_USB_TEST_MODE */
#define  USBHS_UD_TEST_EN           0x80
#define  USBHS_UD_TEST_SE0NAK       0x08
#define  USBHS_UD_TEST_PKT          0x04
#define  USBHS_UD_TEST_K            0x02
#define  USBHS_UD_TEST_J            0x01

/* R16_USB_LPM_DATA */
#define  USBHS_UD_LPM_BUSY          0x8000
#define  USBHS_UD_LPM_DATA          0x07FF

/* R8_USB_INT_FG */
#define  USBHS_UDIF_FIFO_OV         0x80
#define  USBHS_UDIF_LINK_RDY        0x40
#define  USBHS_UDIF_RX_SOF          0x20
#define  USBHS_UDIF_TRANSFER        0x10
#define  USBHS_UDIF_RTX_ACT         0x10
#define  USBHS_UDIF_LPM_ACT         0x08
#define  USBHS_UDIF_BUS_SLEEP       0x04
#define  USBHS_UDIF_SUSPEND         0x02
#define  USBHS_UDIF_BUS_RST         0x01

/* R8_USB_INT_ST */
#define  USBHS_UDIS_EP_DIR          0x10
#define  USBHS_UDIS_EP_ID_MASK      0x07

/* R8_USB_MIS_ST */
#define  USBHS_UDMS_HS_MOD          0x80
#define  USBHS_UDMS_SUSP_REQ        0x10
#define  USBHS_UDMS_SIE_FREE        0x08
#define  USBHS_UDMS_SLEEP           0x04
#define  USBHS_UDMS_SUSPEND         0x02
#define  USBHS_UDMS_READY           0x01

/* R16_USB_FRAME_NO */
#define  USBHS_UD_MFRAME_NO         0xE000
#define  USBHS_UD_FRAME_NO          0x07FF

/* R16_USB_BUS */
#define  USBHS_USB_DM_ST            0x08
#define  USBHS_USB_DP_ST            0x04
#define  USB_WAKEUP                 0x01

/* R16_UEP_TX_EN */
#define USBHS_UEP0_T_EN             0x0001
#define USBHS_UEP1_T_EN             0x0002
#define USBHS_UEP2_T_EN             0x0004
#define USBHS_UEP3_T_EN             0x0008
#define USBHS_UEP4_T_EN             0x0010
#define USBHS_UEP5_T_EN             0x0020
#define USBHS_UEP6_T_EN             0x0040
#define USBHS_UEP7_T_EN             0x0080
#define USBHS_UEP8_T_EN             0x0100
#define USBHS_UEP9_T_EN             0x0200
#define USBHS_UEP10_T_EN            0x0400
#define USBHS_UEP11_T_EN            0x0800
#define USBHS_UEP12_T_EN            0x1000
#define USBHS_UEP13_T_EN            0x2000
#define USBHS_UEP14_T_EN            0x4000
#define USBHS_UEP15_T_EN            0x8000

/* R16_UEP_RX_EN */
#define USBHS_UEP0_R_EN             0x0001
#define USBHS_UEP1_R_EN             0x0002
#define USBHS_UEP2_R_EN             0x0004
#define USBHS_UEP3_R_EN             0x0008
#define USBHS_UEP4_R_EN             0x0010
#define USBHS_UEP5_R_EN             0x0020
#define USBHS_UEP6_R_EN             0x0040
#define USBHS_UEP7_R_EN             0x0080
#define USBHS_UEP8_R_EN             0x0100
#define USBHS_UEP9_R_EN             0x0200
#define USBHS_UEP10_R_EN            0x0400
#define USBHS_UEP11_R_EN            0x0800
#define USBHS_UEP12_R_EN            0x1000
#define USBHS_UEP13_R_EN            0x2000
#define USBHS_UEP14_R_EN            0x4000
#define USBHS_UEP15_R_EN            0x8000

/* R16_UEP_T_TOG_AUTO */
#define USBHS_UEP_T_TOG_AUTO        0xFF

/* R16_UEP_R_TOG_AUTO */
#define USBHS_UEP_R_TOG_AUTO        0xFF

/* R8_UEP_T_BURST */
#define USBHS_UEP_T_BURST_EN        0xFF

/* R8_UEP_T_BURST_MODE */
#define USBHS_UEP_T_BURST_MODE      0xFF

/* R8_UEP_R_BURST */
#define USBHS_UEP_R_BURST_EN        0xFF

/* R8_UEP_R_RES_MODE */
#define USBHS_UEP_R_RES_MODE        0xFF

/* R32_UEP_AF_MODE */
#define USBHS_UEP_T_AF              0xFE

/* R32_UEP0_DMA */
#define UEPn_DMA                    0xFFFFFF

/* R32_UEPn_RX_DMA */
#define UEPn_RX_DMA                 0xFFFFFF

/* R32_UEPn_TX_DMA */
#define UEPn_TX_DMA                 0xFFFFFF

/* R32_UEPn_MAX_LEN */
#define USBHS_UEP0_MAX_LEN          0x007F
#define USBHS_UEPn_MAX_LEN          0x07FF

/* R16_UEPn_RX_LEN */
#define USBHS_UEP0_RX_LEN           0x007F

/* R16_UEPn_RX_LEN */
#define USBHS_UEPn_RX_LEN           0xFFFF

/* R16_UEPn_R_SIZE */
#define USBHS_UEPn_R_SIZE           0xFFFF

/* R16_UEP0_T_LEN */
#define USBHS_UEP0_T_LEN            0x7F

/**R16_UEPn_T_LEN**/ 
#define USBHS_UEPn_T_LEN            0xFFFF

/**R8_UEPn_TX_CTRL**/ 
#define USBHS_UEP_T_DONE            0x80
#define USBHS_UEP_T_NAK_ACT         0x40
#define USBHS_UEP_T_TOG_MASK        0x0C
#define USBHS_UEP_T_TOG_MDATA       0x0C
#define USBHS_UEP_T_TOG_DATA2       0x08
#define USBHS_UEP_T_TOG_DATA1       0x04
#define USBHS_UEP_T_TOG_DATA0       0x00
#define USBHS_UEP_T_RES_MASK        0x03
#define USBHS_UEP_T_RES_ACK         0x02
#define USBHS_UEP_T_RES_STALL       0x01
#define USBHS_UEP_T_RES_NAK         0x00

/**R8_UEP0_RX_CTRL**/ 

/**R8_UEPn_RX_CTRL**/ 
#define USBHS_UEP_R_DONE            0x80
#define USBHS_UEP_R_NAK_ACT         0x40
#define USBHS_UEP_R_NAK_TOG         0x20
#define USBHS_UEP_R_TOG_MATCH       0x10
#define USBHS_UEP_R_SETUP_IS        0x08
#define USBHS_UEP_R_TOG_MASK        0x0C
#define USBHS_UEP_R_TOG_MDATA       0x0C
#define USBHS_UEP_R_TOG_DATA2       0x08
#define USBHS_UEP_R_TOG_DATA1       0x04
#define USBHS_UEP_R_TOG_DATA0       0x00
#define USBHS_UEP_R_RES_MASK        0x03
#define USBHS_UEP_R_RES_ACK         0x02
#define USBHS_UEP_R_RES_STALL       0x01
#define USBHS_UEP_R_RES_NAK         0x00
 
/* R16_UEP_T_ISO */
#define  USBHS_UEP1_T_FIFO_EN       0x0200
#define  USBHS_UEP2_T_FIFO_EN       0x0400
#define  USBHS_UEP3_T_FIFO_EN       0x0800
#define  USBHS_UEP4_T_FIFO_EN       0x1000
#define  USBHS_UEP5_T_FIFO_EN       0x2000
#define  USBHS_UEP6_T_FIFO_EN       0x4000
#define  USBHS_UEP7_T_FIFO_EN       0x8000
#define  USBHS_UEP1_T_ISO_EN        0x0002
#define  USBHS_UEP2_T_ISO_EN        0x0004
#define  USBHS_UEP3_T_ISO_EN        0x0008
#define  USBHS_UEP4_T_ISO_EN        0x0010
#define  USBHS_UEP5_T_ISO_EN        0x0020
#define  USBHS_UEP6_T_ISO_EN        0x0040
#define  USBHS_UEP7_T_ISO_EN        0x0080

/* R16_UEP_R_ISO */
#define  USBHS_UEP1_R_FIFO_EN       0x0200
#define  USBHS_UEP2_R_FIFO_EN       0x0400
#define  USBHS_UEP3_R_FIFO_EN       0x0800
#define  USBHS_UEP4_R_FIFO_EN       0x1000
#define  USBHS_UEP5_R_FIFO_EN       0x2000
#define  USBHS_UEP6_R_FIFO_EN       0x4000
#define  USBHS_UEP7_R_FIFO_EN       0x8000
#define  USBHS_UEP1_R_ISO_EN        0x0002
#define  USBHS_UEP2_R_ISO_EN        0x0004
#define  USBHS_UEP3_R_ISO_EN        0x0008
#define  USBHS_UEP4_R_ISO_EN        0x0010
#define  USBHS_UEP5_R_ISO_EN        0x0020
#define  USBHS_UEP6_R_ISO_EN        0x0040
#define  USBHS_UEP7_R_ISO_EN        0x0080
 
/* R32_UEPn_RX_FIFO */
#define  USBHS_UEP_RX_FIFO_E        0xFF00
#define  USBHS_UEP_RX_FIFO_S        0x00FF

/* R32_UEPn_TX_FIFO */
#define  USBHS_UEP_TX_FIFO_E        0xFF00
#define  USBHS_UEP_TX_FIFO_S        0x00FF


/* USB high speed host register  */
/* R8_UH_CFG */
#define  USBHS_UH_LPM_EN            0x80
#define  USBHS_UH_FORCE_FS          0x40
#define  USBHS_UH_SOF_EN            0x20
#define  USBHS_UH_DMA_EN            0x10
#define  USBHS_UH_PHY_SUSPENDM      0x08
#define  USBHS_UH_CLR_ALL           0x04
#define  USBHS_RST_SIE              0x02
#define  USBHS_RST_LINK             0x01

/* R8_UH_INT_EN */
#define  USBHS_UHIE_FIFO_OVER       0x80
#define  USBHS_UHIE_TX_HALT         0x40
#define  USBHS_UHIE_SOF_ACT         0x20
#define  USBHS_UHIE_TRANSFER        0x10
#define  USBHS_UHIE_RESUME_ACT      0x08
#define  USBHS_UHIE_WKUP_ACT        0x04

/* R8_UH_DEV_AD */
#define  USBHS_UH_DEV_ADDR          0x7F

/* R32_UH_CONTROL */
#define  USBHS_UH_RX_NO_RES         0x800000
#define  USBHS_UH_TX_NO_RES         0x400000
#define  USBHS_UH_RX_NO_DATA        0x200000
#define  USBHS_UH_TX_NO_DATA        0x100000
#define  USBHS_UH_PRE_PID_EN        0x080000
#define  USBHS_UH_SPLIT_VALID       0x040000
#define  USBHS_UH_LPM_VALID         0x020000
#define  USBHS_UH_HOST_ACTION       0x010000
#define  USBHS_UH_BUF_MODE          0x0400
#define  USBHS_UH_T_TOG_MASK        0x0300
#define  USBHS_UH_T_TOG_MDATA       0x0300
#define  USBHS_UH_T_TOG_DATA2       0x0200
#define  USBHS_UH_T_TOG_DATA1       0x0100
#define  USBHS_UH_T_TOG_DATA0       0x0000
#define  USBHS_UH_T_ENDP_MASK       0xF0
#define  USBHS_UH_T_TOKEN_MASK      0x0F

/* R8_UH_INT_FLAG */
#define  USBHS_UHIF_FIFO_OVER       0x80
#define  USBHS_UHIF_TX_HALT         0x40
#define  USBHS_UHIF_SOF_ACT         0x20
#define  USBHS_UHIF_TRANSFER        0x10
#define  USBHS_UHIF_RESUME_ACT      0x08
#define  USBHS_UHIF_WKUP_ACT        0x04

/* R8_UH_INT_ST */
#define  USBHS_UHIF_PORT_RX_RESUME  0x10
#define  USBHS_UH_R_TOKEN_MASK      0x0F

/* R8_UH_MIS_ST */
#define  USBHS_UHMS_BUS_SE0         0x80
#define  USBHS_UHMS_BUS_J           0x40
#define  USBHS_UHMS_LINESTATE       0x30
#define  USBHS_UHMS_USB_WAKEUP      0x08
#define  USBHS_UHMS_SOF_ACT         0x04
#define  USBHS_UHMS_SOF_PRE         0x02
#define  USBHS_UHMS_SOF_FREE        0x01

/* R32_UH_LPM_DATA */
#define  USBHS_UH_LPM_DATA          0x07FF

/* R32_UH_SPLIT_DATA */
#define  USBHS_UH_SPLIT_DATA        0x07FFFF

/* R32_UH_FRAME */
#define  USBHS_UH_SOF_CNT_CLR       0x02000000
#define  USBHS_UH_SOF_CNT_EN        0x01000000
#define  USBHS_UH_MFRAME_NO         0x070000
#define  USBHS_UH_FRAME_NO          0x07FF

/* R32_UH_TX_LEN */
#define  USBHS_UH_TX_LEN            0x07FF

/* R32_UH_RX_LEN */
#define  USBHS_UH_RX_LEN            0x07FF

/* R32_UH_RX_MAX_LEN */
#define  USBHS_UH_RX_MAX_LEN        0x07FF

/* R32_UH_RX_DMA */
#define  USBHS_R32_UH_RX_DMA        0x01FFFF

/* R32_UH_TX_DMA */
#define  USBHS_R32_UH_TX_DMA        0x01FFFF

/* R32_UH_PORT_CTRL */
#define  USBHS_UH_BUS_RST_LONG      0x010000
#define  USBHS_UH_PORT_SLEEP_BESL   0xF000
#define  USBHS_UH_CLR_PORT_SLEEP    0x0100
#define  USBHS_UH_CLR_PORT_CONNECT  0x20
#define  USBHS_UH_CLR_PORT_EN       0x10
#define  USBHS_UH_SET_PORT_SLEEP    0x08
#define  USBHS_UH_CLR_PORT_SUSP     0x04
#define  USBHS_UH_SET_PORT_SUSP     0x02
#define  USBHS_UH_SET_PORT_RESET    0x01

/* R8_UH_PORT_CFG */
#define  USBHS_UH_PD_EN             0x80
#define  USBHS_UH_HOST_EN           0x01

/* R8_UH_PORT_INT_EN */
#define  USBHS_UHIE_PORT_SLP        0x20
#define  USBHS_UHIE_PORT_RESET      0x10
#define  USBHS_UHIE_PORT_SUSP       0x04
#define  USBHS_UHIE_PORT_EN         0x02
#define  USBHS_UHIE_PORT_CONNECT    0x01

/* R8_UH_PORT_TEST_CT */
#define  USBHS_UH_TEST_SE0_NAK      0x10
#define  USBHS_UH_TEST_PACKET       0x08
#define  USBHS_UH_TEST_FORCE_EN     0x04
#define  USBHS_UH_TEST_K            0x02
#define  USBHS_UH_TEST_J            0x01

/* R16_UH_PORT_ST */
#define  USBHS_UHIS_PORT_TEST       0x0800
#define  USBHS_UHIS_PORT_SPEED_MASK 0x0600
#define  USBHS_UHIS_PORT_HS         0x0400
#define  USBHS_UHIS_PORT_LS         0x0200
#define  USBHS_UHIS_PORT_FS         0x0000
#define  USBHS_UHIS_PORT_SLP        0x20
#define  USBHS_UHIS_PORT_RST        0x10
#define  USBHS_UHIS_PORT_SUSP       0x04
#define  USBHS_UHIS_PORT_EN         0x02
#define  USBHS_UHIS_PORT_CONNECT    0x01

/* R8_UH_PORT_CHG */
#define  USBHS_UHIF_PORT_SLP        0x20
#define  USBHS_UHIF_PORT_RESET      0x10
#define  USBHS_UHIF_PORT_SUSP       0x04
#define  USBHS_UHIF_PORT_EN         0x02
#define  USBHS_UHIF_PORT_CONNECT    0x01

/* R32_UH_BC_CTRL */
#define  UDM_VSRC_ACT               0x0400
#define  UDM_BC_VSRC                0x0200
#define  UDP_BC_VSRC                0x0100
#define  BC_AUTO_MODE               0x40
#define  UDM_BC_CMPE                0x20
#define  UDP_BC_CMPE                0x10
#define  UDM_BC_CMPO                0x02
#define  UDP_BC_CMPO                0x01

/*******************************************************************************/
/* USBFS Related Register Macro Definition */

/* R8_USB_CTRL */
#define USBFS_UC_HOST_MODE          0x80
#define USBFS_UC_LOW_SPEED          0x40
#define USBFS_UC_DEV_PU_EN          0x20
#define USBFS_UC_SYS_CTRL_MASK      0x30
#define USBFS_UC_SYS_CTRL0          0x00
#define USBFS_UC_SYS_CTRL1          0x10
#define USBFS_UC_SYS_CTRL2          0x20
#define USBFS_UC_SYS_CTRL3          0x30
#define USBFS_UC_INT_BUSY           0x08
#define USBFS_UC_RESET_SIE          0x04
#define USBFS_UC_CLR_ALL            0x02
#define USBFS_UC_DMA_EN             0x01

/* R8_USB_INT_EN */
#define USBFS_UIE_DEV_SOF           0x80
#define USBFS_UIE_DEV_NAK           0x40
#define USBFS_U_1WIRE_MODE          0x20
#define USBFS_UIE_FIFO_OV           0x10
#define USBFS_UIE_HST_SOF           0x08
#define USBFS_UIE_SUSPEND           0x04
#define USBFS_UIE_TRANSFER          0x02
#define USBFS_UIE_DETECT            0x01
#define USBFS_UIE_BUS_RST           0x01

/* R8_USB_DEV_AD */
#define USBFS_UDA_GP_BIT            0x80
#define USBFS_USB_ADDR_MASK         0x7F

/* R8_USB_MIS_ST */
#define USBFS_UMS_SOF_PRES          0x80
#define USBFS_UMS_SOF_ACT           0x40
#define USBFS_UMS_SIE_FREE          0x20
#define USBFS_UMS_R_FIFO_RDY        0x10
#define USBFS_UMS_BUS_RESET         0x08
#define USBFS_UMS_SUSPEND           0x04
#define USBFS_UMS_DM_LEVEL          0x02
#define USBFS_UMS_DEV_ATTACH        0x01

/* R8_USB_INT_FG */
#define USBFS_U_IS_NAK              0x80    // RO, indicate current USB transfer is NAK received
#define USBFS_U_TOG_OK              0x40    // RO, indicate current USB transfer toggle is OK
#define USBFS_U_SIE_FREE            0x20    // RO, indicate USB SIE free status
#define USBFS_UIF_FIFO_OV           0x10    // FIFO overflow interrupt flag for USB, direct bit address clear or write 1 to clear
#define USBFS_UIF_HST_SOF           0x08    // host SOF timer interrupt flag for USB host, direct bit address clear or write 1 to clear
#define USBFS_UIF_SUSPEND           0x04    // USB suspend or resume event interrupt flag, direct bit address clear or write 1 to clear
#define USBFS_UIF_TRANSFER          0x02    // USB transfer completion interrupt flag, direct bit address clear or write 1 to clear
#define USBFS_UIF_DETECT            0x01    // device detected event interrupt flag for USB host mode, direct bit address clear or write 1 to clear
#define USBFS_UIF_BUS_RST           0x01    // bus reset event interrupt flag for USB device mode, direct bit address clear or write 1 to clear

/* R8_USB_INT_ST */
#define USBFS_UIS_IS_NAK            0x80      // RO, indicate current USB transfer is NAK received for USB device mode
#define USBFS_UIS_TOG_OK            0x40      // RO, indicate current USB transfer toggle is OK
#define USBFS_UIS_TOKEN_MASK        0x30      // RO, bit mask of current token PID code received for USB device mode
#define USBFS_UIS_TOKEN_OUT         0x00
#define USBFS_UIS_TOKEN_SOF         0x10
#define USBFS_UIS_TOKEN_IN          0x20
#define USBFS_UIS_TOKEN_SETUP       0x30
// bUIS_TOKEN1 & bUIS_TOKEN0: current token PID code received for USB device mode
//   00: OUT token PID received
//   01: SOF token PID received
//   10: IN token PID received
//   11: SETUP token PID received
#define USBFS_UIS_ENDP_MASK         0x0F      // RO, bit mask of current transfer endpoint number for USB device mode
#define USBFS_UIS_H_RES_MASK        0x0F      // RO, bit mask of current transfer handshake response for USB host mode: 0000=no response, time out from device, others=handshake response PID received

/* R32_USB_OTG_CR */
#define USBFS_CR_SESS_VTH           0x20
#define USBFS_CR_VBUS_VTH           0x10
#define USBFS_CR_OTG_EN             0x08
#define USBFS_CR_IDPU               0x04
#define USBFS_CR_CHARGE_VBUS        0x02
#define USBFS_CR_DISCHAR_VBUS       0x01

/* R32_USB_OTG_SR */
#define USBFS_SR_ID_DIG             0x08
#define USBFS_SR_SESS_END           0x04
#define USBFS_SR_SESS_VLD           0x02
#define USBFS_SR_VBUS_VLD           0x01

/* R8_UDEV_CTRL */
#define USBFS_UD_PD_DIS             0x80      // disable USB UDP/UDM pulldown resistance: 0=enable pulldown, 1=disable
#define USBFS_UD_DP_PIN             0x20      // ReadOnly: indicate current UDP pin level
#define USBFS_UD_DM_PIN             0x10      // ReadOnly: indicate current UDM pin level
#define USBFS_UD_LOW_SPEED          0x04      // enable USB physical port low speed: 0=full speed, 1=low speed
#define USBFS_UD_GP_BIT             0x02      // general purpose bit
#define USBFS_UD_PORT_EN            0x01      // enable USB physical port I/O: 0=disable, 1=enable

/* R8_UEP4_1_MOD */
#define USBFS_UEP1_RX_EN            0x80      // enable USB endpoint 1 receiving (OUT)
#define USBFS_UEP1_TX_EN            0x40      // enable USB endpoint 1 transmittal (IN)
#define USBFS_UEP1_BUF_MOD          0x10      // buffer mode of USB endpoint 1
#define USBFS_UEP4_RX_EN            0x08      // enable USB endpoint 4 receiving (OUT)
#define USBFS_UEP4_TX_EN            0x04      // enable USB endpoint 4 transmittal (IN)
#define USBFS_UEP4_BUF_MOD          0x01

/* R8_UEP2_3_MOD */
#define USBFS_UEP3_RX_EN            0x80      // enable USB endpoint 3 receiving (OUT)
#define USBFS_UEP3_TX_EN            0x40      // enable USB endpoint 3 transmittal (IN)
#define USBFS_UEP3_BUF_MOD          0x10      // buffer mode of USB endpoint 3
#define USBFS_UEP2_RX_EN            0x08      // enable USB endpoint 2 receiving (OUT)
#define USBFS_UEP2_TX_EN            0x04      // enable USB endpoint 2 transmittal (IN)
#define USBFS_UEP2_BUF_MOD          0x01      // buffer mode of USB endpoint 2

/* R8_UEP5_6_MOD */
#define USBFS_UEP6_RX_EN            0x80      // enable USB endpoint 6 receiving (OUT)
#define USBFS_UEP6_TX_EN            0x40      // enable USB endpoint 6 transmittal (IN)
#define USBFS_UEP6_BUF_MOD          0x10      // buffer mode of USB endpoint 6
#define USBFS_UEP5_RX_EN            0x08      // enable USB endpoint 5 receiving (OUT)
#define USBFS_UEP5_TX_EN            0x04      // enable USB endpoint 5 transmittal (IN)
#define USBFS_UEP5_BUF_MOD          0x01      // buffer mode of USB endpoint 5

/* R8_UEP7_MOD */
#define USBFS_UEP7_RX_EN            0x08      // enable USB endpoint 7 receiving (OUT)
#define USBFS_UEP7_TX_EN            0x04      // enable USB endpoint 7 transmittal (IN)
#define USBFS_UEP7_BUF_MOD          0x01      // buffer mode of USB endpoint 7

/* R8_UEPn_TX_CTRL */
#define USBFS_UEP_T_AUTO_TOG        0x08      // enable automatic toggle after successful transfer completion on endpoint 1/2/3: 0=manual toggle, 1=automatic toggle
#define USBFS_UEP_T_TOG             0x04      // prepared data toggle flag of USB endpoint X transmittal (IN): 0=DATA0, 1=DATA1
#define USBFS_UEP_T_RES_MASK        0x03      // bit mask of handshake response type for USB endpoint X transmittal (IN)
#define USBFS_UEP_T_RES_ACK         0x00
#define USBFS_UEP_T_RES_NONE        0x01
#define USBFS_UEP_T_RES_NAK         0x02
#define USBFS_UEP_T_RES_STALL       0x03
// bUEP_T_RES1 & bUEP_T_RES0: handshake response type for USB endpoint X transmittal (IN)
//   00: DATA0 or DATA1 then expecting ACK (ready)
//   01: DATA0 or DATA1 then expecting no response, time out from host, for non-zero endpoint isochronous transactions
//   10: NAK (busy)
//   11: STALL (error)
// host aux setup

/* R8_UEPn_RX_CTRL, n=0-7 */
#define USBFS_UEP_R_AUTO_TOG        0x08      // enable automatic toggle after successful transfer completion on endpoint 1/2/3: 0=manual toggle, 1=automatic toggle
#define USBFS_UEP_R_TOG             0x04      // expected data toggle flag of USB endpoint X receiving (OUT): 0=DATA0, 1=DATA1
#define USBFS_UEP_R_RES_MASK        0x03      // bit mask of handshake response type for USB endpoint X receiving (OUT)
#define USBFS_UEP_R_RES_ACK         0x00
#define USBFS_UEP_R_RES_NONE        0x01
#define USBFS_UEP_R_RES_NAK         0x02
#define USBFS_UEP_R_RES_STALL       0x03
// RB_UEP_R_RES1 & RB_UEP_R_RES0: handshake response type for USB endpoint X receiving (OUT)
//   00: ACK (ready)
//   01: no response, time out to host, for non-zero endpoint isochronous transactions
//   10: NAK (busy)
//   11: STALL (error)

/* R8_UHOST_CTRL */
#define USBFS_UH_PD_DIS             0x80      // disable USB UDP/UDM pulldown resistance: 0=enable pulldown, 1=disable
#define USBFS_UH_DP_PIN             0x20      // ReadOnly: indicate current UDP pin level
#define USBFS_UH_DM_PIN             0x10      // ReadOnly: indicate current UDM pin level
#define USBFS_UH_LOW_SPEED          0x04      // enable USB port low speed: 0=full speed, 1=low speed
#define USBFS_UH_BUS_RESET          0x02      // control USB bus reset: 0=normal, 1=force bus reset
#define USBFS_UH_PORT_EN            0x01      // enable USB port: 0=disable, 1=enable port, automatic disabled if USB device detached

/* R32_UH_EP_MOD */
#define USBFS_UH_EP_TX_EN           0x40      // enable USB host OUT endpoint transmittal
#define USBFS_UH_EP_TBUF_MOD        0x10      // buffer mode of USB host OUT endpoint
// bUH_EP_TX_EN & bUH_EP_TBUF_MOD: USB host OUT endpoint buffer mode, buffer start address is UH_TX_DMA
//   0 x:  disable endpoint and disable buffer
//   1 0:  64 bytes buffer for transmittal (OUT endpoint)
//   1 1:  dual 64 bytes buffer by toggle bit bUH_T_TOG selection for transmittal (OUT endpoint), total=128bytes
#define USBFS_UH_EP_RX_EN           0x08      // enable USB host IN endpoint receiving
#define USBFS_UH_EP_RBUF_MOD        0x01      // buffer mode of USB host IN endpoint
// bUH_EP_RX_EN & bUH_EP_RBUF_MOD: USB host IN endpoint buffer mode, buffer start address is UH_RX_DMA
//   0 x:  disable endpoint and disable buffer
//   1 0:  64 bytes buffer for receiving (IN endpoint)
//   1 1:  dual 64 bytes buffer by toggle bit bUH_R_TOG selection for receiving (IN endpoint), total=128bytes

/* R16_UH_SETUP */
#define USBFS_UH_PRE_PID_EN         0x0400      // USB host PRE PID enable for low speed device via hub
#define USBFS_UH_SOF_EN             0x0004      // USB host automatic SOF enable

/* R8_UH_EP_PID */
#define USBFS_UH_TOKEN_MASK         0xF0      // bit mask of token PID for USB host transfer
#define USBFS_UH_ENDP_MASK          0x0F      // bit mask of endpoint number for USB host transfer

/* R8_UH_RX_CTRL */
#define USBFS_UH_R_AUTO_TOG         0x08      // enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle
#define USBFS_UH_R_TOG              0x04      // expected data toggle flag of host receiving (IN): 0=DATA0, 1=DATA1
#define USBFS_UH_R_RES              0x01      // prepared handshake response type for host receiving (IN): 0=ACK (ready), 1=no response, time out to device, for isochronous transactions

/* R8_UH_TX_CTRL */
#define USBFS_UH_T_AUTO_TOG         0x08      // enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle
#define USBFS_UH_T_TOG              0x04      // prepared data toggle flag of host transmittal (SETUP/OUT): 0=DATA0, 1=DATA1
#define USBFS_UH_T_RES              0x01      // expected handshake response type for host transmittal (SETUP/OUT): 0=ACK (ready), 1=no response, time out from device, for isochronous transactions


/*******************************************************************************/
/* Struct Definition */

/* USB Setup Request */
typedef struct __attribute__((packed)) _USB_SETUP_REQ
{
    uint8_t  bRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SETUP_REQ, *PUSB_SETUP_REQ;

/* USB Device Descriptor */
typedef struct __attribute__((packed)) _USB_DEVICE_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} USB_DEV_DESCR, *PUSB_DEV_DESCR;

/* USB Configuration Descriptor */
typedef struct __attribute__((packed)) _USB_CONFIG_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  MaxPower;
} USB_CFG_DESCR, *PUSB_CFG_DESCR;

/* USB Interface Descriptor */
typedef struct __attribute__((packed)) _USB_INTERF_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} USB_ITF_DESCR, *PUSB_ITF_DESCR;

/* USB Endpoint Descriptor */
typedef struct __attribute__((packed)) _USB_ENDPOINT_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint8_t  wMaxPacketSizeL;
    uint8_t  wMaxPacketSizeH;
    uint8_t  bInterval;
} USB_ENDP_DESCR, *PUSB_ENDP_DESCR;

/* USB Configuration Descriptor Set */
typedef struct __attribute__((packed)) _USB_CONFIG_DESCR_LONG
{
    USB_CFG_DESCR  cfg_descr;
    USB_ITF_DESCR  itf_descr;
    USB_ENDP_DESCR endp_descr[ 1 ];
} USB_CFG_DESCR_LONG, *PUSB_CFG_DESCR_LONG;

/* USB HUB Descriptor */
typedef struct __attribute__((packed)) _USB_HUB_DESCR
{
    uint8_t  bDescLength;
    uint8_t  bDescriptorType;
    uint8_t  bNbrPorts;
    uint8_t  wHubCharacteristicsL;
    uint8_t  wHubCharacteristicsH;
    uint8_t  bPwrOn2PwrGood;
    uint8_t  bHubContrCurrent;
    uint8_t  DeviceRemovable;
    uint8_t  PortPwrCtrlMask;
} USB_HUB_DESCR, *PUSB_HUB_DESCR;

/* USB HID Descriptor */
typedef struct __attribute__((packed)) _USB_HID_DESCR
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bDescriptorTypeX;
    uint8_t  wDescriptorLengthL;
    uint8_t  wDescriptorLengthH;
} USB_HID_DESCR, *PUSB_HID_DESCR;

/* USB UDisk */
typedef struct __attribute__((packed)) _UDISK_BOC_CBW
{
    uint32_t mCBW_Sig;
    uint32_t mCBW_Tag;
    uint32_t mCBW_DataLen;
    uint8_t  mCBW_Flag;
    uint8_t  mCBW_LUN;
    uint8_t  mCBW_CB_Len;
    uint8_t  mCBW_CB_Buf[ 16 ];
} UDISK_BOC_CBW, *PXUDISK_BOC_CBW;

/* USB UDisk */
typedef struct __attribute__((packed)) _UDISK_BOC_CSW
{
    uint32_t mCBW_Sig;
    uint32_t mCBW_Tag;
    uint32_t mCSW_Residue;
    uint8_t  mCSW_Status;
} UDISK_BOC_CSW, *PXUDISK_BOC_CSW;


#ifdef __cplusplus
}
#endif

#endif /* __CH32H417_USB_H */
