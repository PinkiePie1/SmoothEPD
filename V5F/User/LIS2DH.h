#ifndef LIS2DH_H
#define LIS2DH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* LIS2DH12TR 三轴加速度计，I2C 模式，接在 CH32H417 的 I2C2 上：
 *   PC0 -> SCL (AF9)
 *   PC1 -> SDA (AF9)
 *
 * 量程固定 ±2 g、高分辨率 12 位，读数单位为 mg（1 mg/LSB）。
 * 初始化里用到了 Delay_Ms()，调用前需先执行 Delay_Init()。
 */

typedef struct
{
    int16_t x;      /* mg，右为 +X */
    int16_t y;      /* mg，上为 +Y */
    int16_t z;      /* mg，垂直向上为 +Z */
} LIS2DH_Accel;

/* 初始化：探测 0x18/0x19 地址、校验 WHO_AM_I、配置 100 Hz + XYZ。
 * 返回 0 成功，负值失败（-1 未找到器件，-2/-3 配置写失败）。 */
int LIS2DH_Init(void);

/* 读取一次三轴数据。
 * 返回 0 读到新样本，1 暂无新样本（accel 不变），负值 I2C 错误。 */
int LIS2DH_Read(LIS2DH_Accel *accel);

/* 进入 power-down。调用后需重新 LIS2DH_Init() 才能读数据。 */
void LIS2DH_Sleep(void);

/* 初始化成功后实际使用的 7 位 I2C 地址：0x18（SA0=0）或 0x19（SA0=1）。 */
uint8_t LIS2DH_Address(void);

#ifdef __cplusplus
}
#endif

#endif
