#include "LIS2DH.h"

#include "ch32h417.h"
#include "debug.h"

/* ---------------------------------------------------------------- 硬件 */

#define LIS2DH_I2C              I2C2
#define LIS2DH_I2C_SPEED        400000u
#define LIS2DH_TIMEOUT          200000u

/* ------------------------------------------------------------ 器件定义 */

#define LIS2DH_ADDR_SA0_LOW     0x18u
#define LIS2DH_ADDR_SA0_HIGH    0x19u

#define LIS2DH_REG_WHO_AM_I     0x0Fu
#define LIS2DH_REG_CTRL_REG1    0x20u
#define LIS2DH_REG_CTRL_REG4    0x23u
#define LIS2DH_REG_STATUS       0x27u
#define LIS2DH_REG_OUT_X_L      0x28u

#define LIS2DH_WHO_AM_I_VALUE   0x33u

/* CTRL_REG1: ODR = 0101 (100 Hz), LPen = 0, Zen = Yen = Xen = 1 */
#define LIS2DH_CTRL1_100HZ_XYZ  0x57u
/* CTRL_REG4: BDU = 1, BLE = 0, FS = 00 (±2 g), HR = 1 */
#define LIS2DH_CTRL4_2G_12BIT   0x88u

#define LIS2DH_STATUS_ZYXDA     0x08u
#define LIS2DH_AUTO_INCREMENT   0x80u

/* ---------------------------------------------------------------- 状态 */

static uint8_t s_addr  = LIS2DH_ADDR_SA0_LOW;
static uint8_t s_ready = 0;

/* ------------------------------------------------------------ I2C 基础 */

static int i2c_wait_flag(uint32_t flag)
{
    uint32_t guard = LIS2DH_TIMEOUT;

    while (I2C_GetFlagStatus(LIS2DH_I2C, flag) == RESET) {
        if (--guard == 0u) {
            return -1;
        }
    }

    return 0;
}

/* 总线空闲后发 START，用于一次新传输的开头 */
static int i2c_start(void)
{
    uint32_t guard = LIS2DH_TIMEOUT;

    while (I2C_GetFlagStatus(LIS2DH_I2C, I2C_FLAG_BUSY) != RESET) {
        if (--guard == 0u) {
            return -2;
        }
    }

    I2C_GenerateSTART(LIS2DH_I2C, ENABLE);
    return i2c_wait_flag(I2C_FLAG_SB);
}

/* 重复 START，总线仍处于 BUSY，不能等空闲 */
static int i2c_restart(void)
{
    I2C_GenerateSTART(LIS2DH_I2C, ENABLE);
    return i2c_wait_flag(I2C_FLAG_SB);
}

static void i2c_stop(void)
{
    I2C_GenerateSTOP(LIS2DH_I2C, ENABLE);
}

static void i2c_clear_addr(void)
{
    (void)LIS2DH_I2C->STAR1;
    (void)LIS2DH_I2C->STAR2;
}

/* 只发地址并等 ADDR，ADDR 由调用者按读/写时序清除 */
static int i2c_send_addr(uint8_t direction)
{
    uint32_t guard = LIS2DH_TIMEOUT;

    I2C_Send7bitAddress(LIS2DH_I2C, (uint8_t)(s_addr << 1), direction);

    while (I2C_GetFlagStatus(LIS2DH_I2C, I2C_FLAG_ADDR) == RESET) {
        if (I2C_GetFlagStatus(LIS2DH_I2C, I2C_FLAG_AF) != RESET) {
            I2C_ClearFlag(LIS2DH_I2C, I2C_FLAG_AF);
            i2c_stop();
            return -4;
        }
        if (--guard == 0u) {
            i2c_stop();
            return -5;
        }
    }

    return 0;
}

/* -------------------------------------------------------- 寄存器读写 */

static int reg_write(uint8_t reg, uint8_t value)
{
    int ret;

    ret = i2c_start();
    if (ret != 0) {
        return ret;
    }

    ret = i2c_send_addr(I2C_Direction_Transmitter);
    if (ret != 0) {
        return ret;
    }
    i2c_clear_addr();

    ret = i2c_wait_flag(I2C_FLAG_TXE);
    if (ret != 0) {
        i2c_stop();
        return ret;
    }
    I2C_SendData(LIS2DH_I2C, reg);

    ret = i2c_wait_flag(I2C_FLAG_TXE);
    if (ret != 0) {
        i2c_stop();
        return ret;
    }
    I2C_SendData(LIS2DH_I2C, value);

    ret = i2c_wait_flag(I2C_FLAG_BTF);
    if (ret != 0) {
        i2c_stop();
        return ret;
    }

    i2c_stop();
    return 0;
}

static int reg_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    int ret;
    uint8_t i;

    ret = i2c_start();
    if (ret != 0) {
        return ret;
    }

    ret = i2c_send_addr(I2C_Direction_Transmitter);
    if (ret != 0) {
        return ret;
    }
    i2c_clear_addr();

    ret = i2c_wait_flag(I2C_FLAG_TXE);
    if (ret != 0) {
        i2c_stop();
        return ret;
    }
    I2C_SendData(LIS2DH_I2C, (uint8_t)((len > 1u) ? (reg | LIS2DH_AUTO_INCREMENT) : reg));

    ret = i2c_wait_flag(I2C_FLAG_BTF);
    if (ret != 0) {
        i2c_stop();
        return ret;
    }

    ret = i2c_restart();
    if (ret != 0) {
        return ret;
    }

    ret = i2c_send_addr(I2C_Direction_Receiver);
    if (ret != 0) {
        return ret;
    }

    if (len == 1u) {
        /* 单字节读：必须在清 ADDR 之前关 ACK，否则末字节会被 ACK */
        I2C_AcknowledgeConfig(LIS2DH_I2C, DISABLE);
        i2c_clear_addr();

        ret = i2c_wait_flag(I2C_FLAG_RXNE);
        if (ret == 0) {
            buf[0] = I2C_ReceiveData(LIS2DH_I2C);
        }

        i2c_stop();
        I2C_AcknowledgeConfig(LIS2DH_I2C, ENABLE);
        return ret;
    }

    i2c_clear_addr();

    for (i = 0; i < len; ++i) {
        if (i == (uint8_t)(len - 1u)) {
            I2C_AcknowledgeConfig(LIS2DH_I2C, DISABLE);
        }

        ret = i2c_wait_flag(I2C_FLAG_RXNE);
        if (ret != 0) {
            i2c_stop();
            I2C_AcknowledgeConfig(LIS2DH_I2C, ENABLE);
            return ret;
        }
        buf[i] = I2C_ReceiveData(LIS2DH_I2C);
    }

    i2c_stop();
    I2C_AcknowledgeConfig(LIS2DH_I2C, ENABLE);
    return 0;
}

/* 12 位数据左对齐在 16 位寄存器里，右移 4 位后 1 LSB = 1 mg */
static int16_t raw_to_mg(uint8_t low, uint8_t high)
{
    const int32_t raw = (int32_t)(int16_t)(((uint16_t)high << 8) | low);

    return (int16_t)(raw >> 4);
}

/* ---------------------------------------------------------------- 接口 */

int LIS2DH_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    I2C_InitTypeDef  i2c  = {0};
    uint8_t who = 0;
    uint8_t found = 0;
    uint8_t addr;

    s_ready = 0;
    s_addr  = LIS2DH_ADDR_SA0_LOW;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOC | RCC_HB2Periph_AFIO, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_I2C2, ENABLE);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource0, GPIO_AF9);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF9);

    gpio.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOC, &gpio);

    i2c.I2C_ClockSpeed          = LIS2DH_I2C_SPEED;
    i2c.I2C_Mode                = I2C_Mode_I2C;
    i2c.I2C_DutyCycle           = I2C_DutyCycle_16_9;
    i2c.I2C_OwnAddress1         = 0x00;
    i2c.I2C_Ack                 = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(LIS2DH_I2C, &i2c);
    I2C_Cmd(LIS2DH_I2C, ENABLE);

    Delay_Ms(10);

    for (addr = LIS2DH_ADDR_SA0_LOW; addr <= LIS2DH_ADDR_SA0_HIGH; ++addr) {
        s_addr = addr;
        who = 0;
        if ((reg_read(LIS2DH_REG_WHO_AM_I, &who, 1) == 0) && (who == LIS2DH_WHO_AM_I_VALUE)) {
            found = 1;
            break;
        }
    }

    if (found == 0u) {
        return -1;
    }

    if (reg_write(LIS2DH_REG_CTRL_REG4, LIS2DH_CTRL4_2G_12BIT) != 0) {
        return -2;
    }
    if (reg_write(LIS2DH_REG_CTRL_REG1, LIS2DH_CTRL1_100HZ_XYZ) != 0) {
        return -3;
    }

    s_ready = 1;
    return 0;
}

int LIS2DH_Read(LIS2DH_Accel *accel)
{
    uint8_t status = 0;
    uint8_t raw[6];
    int ret;

    if ((s_ready == 0u) || (accel == 0)) {
        return -1;
    }

    ret = reg_read(LIS2DH_REG_STATUS, &status, 1);
    if (ret != 0) {
        return -2;
    }
    if ((status & LIS2DH_STATUS_ZYXDA) == 0u) {
        return 1;
    }

    ret = reg_read(LIS2DH_REG_OUT_X_L, raw, 6);
    if (ret != 0) {
        return -3;
    }

    accel->x = raw_to_mg(raw[0], raw[1]);
    accel->y = raw_to_mg(raw[2], raw[3]);
    accel->z = raw_to_mg(raw[4], raw[5]);

    return 0;
}

void LIS2DH_Sleep(void)
{
    if (s_ready == 0u) {
        return;
    }

    (void)reg_write(LIS2DH_REG_CTRL_REG1, 0x00u);
    s_ready = 0;
}

uint8_t LIS2DH_Address(void)
{
    return s_addr;
}
