#ifndef SANDSIM_H
#define SANDSIM_H

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* 墨水屏 152x152，粒子按 4x4 像素画，128x128 模拟区位于左上角 */
#define NumberOfParticles 2500U
#define ParticleRadius 0.018f
#define Spacing 0.045f
#define CellNumX 64U
#define CellNumY 64U
#define CellCount (CellNumX * CellNumY)
#define dt 0.010f //粒子慢一点，给足够时间刷白刷黑
#define BOUNCYNESS -0.9f

#define overRelaxiation 1.9f
#define stiffnessCoefficient 1.2f

#define XID(n) (2 * (n))
#define YID(n) (2 * (n) + 1)
#define INDEX(x, y) ((x) * CellNumY + (y))

void ParticleIntegrate(float xAcceleration, float yAcceleration);
void PushParticlesApart(uint16_t nIters);
void density_update(void);
void particles_to_grid(void);
void compute_grid_forces(uint16_t nIters);
void grid_to_particles(void);
void InitParticles(void);

/* 给渲染用：粒子位置（x,y 交错）和网格类型 */
extern float particlePos[NumberOfParticles * 2];
extern float particleVel[NumberOfParticles * 2];

#define CELL_FLUID 0U
#define CELL_AIR   1U
#define CELL_SOLID 2U
uint8_t sim_cell_type(uint16_t x, uint16_t y);

#endif
