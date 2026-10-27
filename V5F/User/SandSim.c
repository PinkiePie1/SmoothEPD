#pragma GCC optimize("O3,unroll-loops")
#include "SandSim.h"
#include "charlie.h"

/* RV32IMAFC 有单精度硬件 FPU，但 nano libm 的 sqrtf 会链软件实现（200+ 字节）。
 * 改用内联 fsqrt.s，不依赖编译选项。 */
static inline float fsqrt_fast(float x)
{
#if defined(__riscv)
    float r;
    __asm__ volatile("fsqrt.s %0, %1" : "=f"(r) : "f"(x));
    return r;
#else
    return sqrtf(x);   /* 主机上模拟时用 libm */
#endif
}
const float invertSpacing = 1.0f / Spacing;
const float particleInvSpacing = 1.0f / (2.2f * ParticleRadius);

const float minX = Spacing + ParticleRadius;
const float maxX = (CellNumX - 1U) * Spacing - ParticleRadius;
const float minY = Spacing + ParticleRadius;
const float maxY = (CellNumY - 1U) * Spacing - ParticleRadius;

#define FLUID_CELL 0U
#define AIR_CELL 1U
#define SOLID_CELL 2U

float particlePos[NumberOfParticles * 2];
float particleVel[NumberOfParticles * 2];

static float uVel[CellCount];
static float vVel[CellCount];

#define SIM_GRID_BYTES       (CellCount * sizeof(float))
#define SIM_DENSITY_ADDR     0x20142000u
#define SIM_U_WEIGHT_ADDR    0x2014C000u
#define SIM_V_WEIGHT_ADDR    0x20156000u
#define SIM_U_PREV_ADDR      0x20160000u
#define SIM_INV_S_ADDR       0x20173000u
#define SIM_NEIGHBOR_ADDR    0x2017C400u

static float *const uPrev = (float *)SIM_U_PREV_ADDR;
static float vPrev[CellCount];
static float *const invSCoeff = (float *)SIM_INV_S_ADDR;
static uint8_t *const solidNeighbours = (uint8_t *)SIM_NEIGHBOR_ADDR;
static float *const uWeight = (float *)SIM_U_WEIGHT_ADDR;
static float *const vWeight = (float *)SIM_V_WEIGHT_ADDR;
static float *const particleDensity = (float *)SIM_DENSITY_ADDR;
_Static_assert(SIM_V_WEIGHT_ADDR + SIM_GRID_BYTES <= 0x20170000u,
               "shared particle buffers exceed the free region");
static uint8_t cellType[CellCount / 4U];
static float particleRestDensity = 0.0f;

static inline uint8_t cell_type_get(uint16_t i)
{
    return (cellType[i >> 2] >> (2U * (i & 3U))) & 3U;
}

static inline void cell_type_set(uint16_t i, uint8_t value)
{
    const uint8_t shift = (uint8_t)(2U * (i & 3U));
    const uint8_t mask = (uint8_t)(3U << shift);

    cellType[i >> 2] = (uint8_t)((cellType[i >> 2] & (uint8_t)~mask) |
                                 (uint8_t)(value << shift));
}

#define SOLID_MASK(i) (CELL_TYPE(i) != SOLID_CELL)
#define CELL_TYPE(i)  cell_type_get((uint16_t)(i))
/* 分离阶段缓存粒子格号，P2G 前半区作触摸标记，压力阶段再存流体格索引。 */
static uint16_t fluidCells[CellCount];
static uint16_t fluidCellCount;
static uint8_t *const touchState = (uint8_t *)fluidCells;

#define PNumX (CellNumX * 2U)
#define PNumY (CellNumY * 2U)
#define PCellCount (PNumX * PNumY)

#define CellPrefixCountSize (CellCount + 1U)

static uint16_t cellParticleCountPrefix[CellPrefixCountSize];
static uint16_t particlePosId[NumberOfParticles];


void printLocation(uint16_t n);

static inline int clamp_index(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static inline float clampf_local(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

// 输入在本项目中都为非负数
static inline int floor_to_int_nonneg(float value) {
    return (int)value;
}

static void setup_solid_mask(void) {
    /* 每个 2-bit 格都为 AIR(01)，再覆盖最外圈 SOLID。 */
    memset(cellType, 0x55, sizeof(cellType));

    for (uint16_t x = 0; x < CellNumX; ++x) {
        cell_type_set(INDEX(x, 0), SOLID_CELL);
        cell_type_set(INDEX(x, CellNumY - 1U), SOLID_CELL);
    }
    for (uint16_t y = 1; y < CellNumY - 1U; ++y) {
        cell_type_set(INDEX(0, y), SOLID_CELL);
        cell_type_set(INDEX(CellNumX - 1U, y), SOLID_CELL);
    }
}

void InitParticles() {
    memset(particleVel, 0, sizeof(particleVel));
    memset(uVel, 0, sizeof(uVel));
    memset(vVel, 0, sizeof(vVel));
    memset(uPrev, 0, SIM_GRID_BYTES);
    memset(vPrev, 0, sizeof(vPrev));
    memset(uWeight, 0, SIM_GRID_BYTES);
    memset(vWeight, 0, SIM_GRID_BYTES);
    memset(particleDensity, 0, SIM_GRID_BYTES);
    particleRestDensity = 0.0f;
    setup_solid_mask();

    const float h = Spacing;
    const float r = ParticleRadius;
    const float dx = 2.0f * r;
    const float dy = 0.86602540378f * dx; // sqrt(3)/2

    uint16_t p_num = 0;
    for (uint16_t i = 0; i < CellNumX && p_num < NumberOfParticles; i++) {
        for (uint16_t j = 0; j < CellNumY && p_num < NumberOfParticles; j++) {
            float px = h + r + dx * (float)i + ((j % 2U == 0U) ? 0.0f : r);
            float py = h + r + dy * (float)j;
            if (px > (CellNumX - 1U) * h - r || py > (CellNumY - 1U) * h - r) {
                continue;
            }
            particlePos[XID(p_num)] = px;
            particlePos[YID(p_num)] = py;
            p_num++;
        }
    }

    for (; p_num < NumberOfParticles; p_num++) {
        particlePos[XID(p_num)] = h + r;
        particlePos[YID(p_num)] = h + r;
    }
}

void ParticleIntegrate(float xAcceleration, float yAcceleration) {
    const float axdt = xAcceleration * dt;
    const float aydt = yAcceleration * dt;
    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        const uint16_t xid = XID(i);
        const uint16_t yid = YID(i);
        float vx = particleVel[xid] + axdt;
        float vy = particleVel[yid] + aydt;
        float x = particlePos[xid] + vx * dt;
        float y = particlePos[yid] + vy * dt;

        if (x < minX) {
            x = minX;
            vx *= BOUNCYNESS;
        }
        if (x > maxX) {
            x = maxX;
            vx *= BOUNCYNESS;
        }
        if (y < minY) {
            y = minY;
            vy *= BOUNCYNESS;
        }
        if (y > maxY) {
            y = maxY;
            vy *= BOUNCYNESS;
        }

        particleVel[xid] = vx;
        particleVel[yid] = vy;
        particlePos[xid] = x;
        particlePos[yid] = y;
    }
}


void PushParticlesApart(uint16_t nIters) {
    memset(cellParticleCountPrefix, 0, sizeof(cellParticleCountPrefix));

    // --- build grid ---
    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        float px = particlePos[XID(i)];
        float py = particlePos[YID(i)];

        uint16_t xi = (uint16_t)clamp_index((int)(px * invertSpacing), 0, (int)CellNumX - 1);
        uint16_t yi = (uint16_t)clamp_index((int)(py * invertSpacing), 0, (int)CellNumY - 1);
        uint16_t cellNr = INDEX(xi, yi);

        fluidCells[i] = cellNr;
        cellParticleCountPrefix[cellNr]++;
    }

    uint16_t prefix = 0;
    for (uint16_t i = 0; i < CellCount; i++) {
        prefix += cellParticleCountPrefix[i];
        cellParticleCountPrefix[i] = prefix;
    }
    cellParticleCountPrefix[CellCount] = prefix;

    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        uint16_t cellNr = fluidCells[i];
        uint16_t gridIndex = --cellParticleCountPrefix[cellNr];
        particlePosId[gridIndex] = i;
    }

    const float minDist  = 2.0f * ParticleRadius;
    const float minDist2 = minDist * minDist;


    for (uint16_t iter = 0; iter < nIters; iter++) {

        for (uint16_t i = 0; i < NumberOfParticles; i++) {


            float px = particlePos[XID(i)];
            float py = particlePos[YID(i)];

            int pxi = clamp_index((int)(px * invertSpacing), 0, (int)CellNumX - 1);
            int pyi = clamp_index((int)(py * invertSpacing), 0, (int)CellNumY - 1);

            int x0 = (pxi > 0) ? pxi - 1 : 0;
            int y0 = (pyi > 0) ? pyi - 1 : 0;
            int x1 = (pxi + 1 < (int)CellNumX) ? pxi + 1 : (int)CellNumX - 1;
            int y1 = (pyi + 1 < (int)CellNumY) ? pyi + 1 : (int)CellNumY - 1;


            float dx_acc = 0.0f;
            float dy_acc = 0.0f;

            /* 单侧处理：只在自己格号 <= 对方格号时扫描，否则这一对已经由对方处理过。
             * 同格内再按 id 排序保证只处理一次。候选数直接减半。 */
            const uint16_t icell = (uint16_t)INDEX((uint16_t)pxi, (uint16_t)pyi);

            for (int xi = x0; xi <= x1; xi++) {
                for (int yi = y0; yi <= y1; yi++) {

                    uint16_t cellNr = INDEX((uint16_t)xi, (uint16_t)yi);
                    if (cellNr < icell) continue;

                    uint16_t firstIdx = cellParticleCountPrefix[cellNr];
                    uint16_t lastIdx  = cellParticleCountPrefix[cellNr + 1U];

                    for (uint16_t j = firstIdx; j < lastIdx; j++) {

                        uint16_t id = particlePosId[j];
                        if (id == i) continue;
                        if (cellNr == icell && id < i) continue;

                        float qx = particlePos[XID(id)];
                        float qy = particlePos[YID(id)];

                        float dx = qx - px;
                        float dy = qy - py;

                        float d2 = dx * dx + dy * dy;
                        if (d2 > minDist2 || d2 == 0.0f) continue;


                        float inv_d = 1.0f / fsqrt_fast(d2);


                        float s = 0.5f * (minDist * inv_d - 1.0f);

                        float ox = dx * s;
                        float oy = dy * s;

                        dx_acc += ox;
                        dy_acc += oy;

                        particlePos[XID(id)] += ox;
                        particlePos[YID(id)] += oy;
                    }
                }
            }

            particlePos[XID(i)] = px - dx_acc;
            particlePos[YID(i)] = py - dy_acc;
        }
    }

    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        float x = particlePos[XID(i)];
        float y = particlePos[YID(i)];

        if (x < minX) {
            x = minX;
            particleVel[XID(i)] *= BOUNCYNESS;
        }
        if (x > maxX) {
            x = maxX;
            particleVel[XID(i)] *= BOUNCYNESS;
        }
        if (y < minY) {
            y = minY;
            particleVel[YID(i)] *= BOUNCYNESS;
        }
        if (y > maxY) {
            y = maxY;
            particleVel[YID(i)] *= BOUNCYNESS;
        }

        particlePos[XID(i)] = x;
        particlePos[YID(i)] = y;
    }
}


/* 双线性采样几何缓存：坐标和小数都用 uint8_t，小数为 8 位定点数。 */
#define SIM_GEOM_X_ADDR      0x20130000u
#define SIM_GEOM_Y_ADDR      0x20133000u
#define SIM_GEOM_TX_ADDR     0x20136000u
#define SIM_GEOM_TY_ADDR     0x2013C000u

static uint8_t (*const bilinX)[NumberOfParticles] = (void *)SIM_GEOM_X_ADDR;
static uint8_t (*const bilinY)[NumberOfParticles] = (void *)SIM_GEOM_Y_ADDR;
static uint8_t (*const bilinTx)[NumberOfParticles] = (void *)SIM_GEOM_TX_ADDR;
static uint8_t (*const bilinTy)[NumberOfParticles] = (void *)SIM_GEOM_TY_ADDR;
_Static_assert(SIM_GEOM_TY_ADDR + 2U * NumberOfParticles * sizeof(uint8_t) <= SIM_DENSITY_ADDR,
               "geometry buffers overlap density");

static inline uint8_t bilin_q_from_float(float value)
{
    return (uint8_t)(value * 255.0f + 0.5f);
}

static inline float bilin_q_to_float(uint8_t value)
{
    return (float)value * (1.0f / 255.0f);
}

static void update_rest_density(void)
{
    if (particleRestDensity != 0.0f) {
        return;
    }

    float sum = 0.0f;
    uint16_t numFluid = 0;
    for (uint16_t i = 0; i < CellCount; i++) {
        if (CELL_TYPE(i) == FLUID_CELL) {
            sum += particleDensity[i];
            numFluid++;
        }
    }
    if (numFluid > 0U) {
        particleRestDensity = sum / (float)numFluid;
    }
}

static void build_bilinear_geom(void)
{
    const float h  = Spacing;
    const float h1 = invertSpacing;
    const float h2 = 0.5f * h;

    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        const float x = clampf_local(particlePos[XID(i)], h, (CellNumX - 1U) * h);
        const float y = clampf_local(particlePos[YID(i)], h, (CellNumY - 1U) * h);

        for (int c = 0; c < 2; c++) {
            const float dx = (c == 0) ? 0.0f : h2;
            const float dy = (c == 0) ? h2 : 0.0f;
            const int x0 = clamp_index(floor_to_int_nonneg((x - dx) * h1), 0, (int)CellNumX - 2);
            const int y0 = clamp_index(floor_to_int_nonneg((y - dy) * h1), 0, (int)CellNumY - 2);

            bilinX[c][i] = (uint8_t)x0;
            bilinY[c][i] = (uint8_t)y0;
            bilinTx[c][i] = bilin_q_from_float(((x - dx) - x0 * h) * h1);
            bilinTy[c][i] = bilin_q_from_float(((y - dy) - y0 * h) * h1);
        }
    }
}

static void restore_boundary_velocity(int component, float *f, const float *prev)
{
    /* 外圈都是固体；u 还恢复 x=1，v 还恢复 y=1。 */
    for (uint16_t y = 0; y < CellNumY; ++y) {
        uint16_t idx = INDEX(0, y);
        f[idx] = prev[idx];
        idx = INDEX(CellNumX - 1U, y);
        f[idx] = prev[idx];
        if (component == 0) {
            idx = INDEX(1, y);
            f[idx] = prev[idx];
        }
    }
    for (uint16_t x = (component == 0) ? 2U : 1U; x < CellNumX - 1U; ++x) {
        uint16_t idx = INDEX(x, 0);
        f[idx] = prev[idx];
        if (component != 0) {
            idx = INDEX(x, 1);
            f[idx] = prev[idx];
        }
        idx = INDEX(x, CellNumY - 1U);
        f[idx] = prev[idx];
    }
}

void particles_to_grid(void) {
    build_bilinear_geom();   /* 本帧的采样几何只算一次，p2g/g2p 共用 */
    memcpy(uPrev, uVel, sizeof(uVel));
    memcpy(vPrev, vVel, sizeof(vVel));
    memset(touchState, 0, CellCount);

    setup_solid_mask();

    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        int xi = clamp_index(floor_to_int_nonneg(particlePos[XID(i)] * invertSpacing), 0, (int)CellNumX - 1);
        int yi = clamp_index(floor_to_int_nonneg(particlePos[YID(i)] * invertSpacing), 0, (int)CellNumY - 1);
        uint16_t cellNr = INDEX((uint16_t)xi, (uint16_t)yi);
        if (CELL_TYPE(cellNr) == AIR_CELL) {
            cell_type_set(cellNr, FLUID_CELL);
        }
    }

    for (int component = 0; component < 2; component++) {
        float *f = (component == 0) ? uVel : vVel;
        float *w = (component == 0) ? uWeight : vWeight;
        const float *prev = (component == 0) ? uPrev : vPrev;
        const uint8_t touch = (uint8_t)(component + 1);

        for (uint16_t i = 0; i < NumberOfParticles; i++) {
            int x0 = bilinX[component][i];
            int y0 = bilinY[component][i];
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float tx = bilin_q_to_float(bilinTx[component][i]);
            float ty = bilin_q_to_float(bilinTy[component][i]);
            float sx = 1.0f - tx;
            float sy = 1.0f - ty;

            float w0 = sx * sy;
            float w1 = tx * sy;
            float w2 = tx * ty;
            float w3 = sx * ty;

            float pv = particleVel[2 * i + (uint16_t)component];
            uint16_t nr0 = INDEX((uint16_t)x0, (uint16_t)y0);
            uint16_t nr1 = INDEX((uint16_t)x1, (uint16_t)y0);
            uint16_t nr2 = INDEX((uint16_t)x1, (uint16_t)y1);
            uint16_t nr3 = INDEX((uint16_t)x0, (uint16_t)y1);

            if (touchState[nr0] != touch) {
                touchState[nr0] = touch;
                f[nr0] = 0.0f;
                w[nr0] = 0.0f;
                if (component == 0) particleDensity[nr0] = 0.0f;
            }
            f[nr0] = fmaf(pv, w0, f[nr0]); w[nr0] += w0;
            if (component == 0) particleDensity[nr0] = fmaf(sx, sy, particleDensity[nr0]);

            if (touchState[nr1] != touch) {
                touchState[nr1] = touch;
                f[nr1] = 0.0f;
                w[nr1] = 0.0f;
                if (component == 0) particleDensity[nr1] = 0.0f;
            }
            f[nr1] = fmaf(pv, w1, f[nr1]); w[nr1] += w1;
            if (component == 0) particleDensity[nr1] = fmaf(tx, sy, particleDensity[nr1]);

            if (touchState[nr2] != touch) {
                touchState[nr2] = touch;
                f[nr2] = 0.0f;
                w[nr2] = 0.0f;
                if (component == 0) particleDensity[nr2] = 0.0f;
            }
            f[nr2] = fmaf(pv, w2, f[nr2]); w[nr2] += w2;
            if (component == 0) particleDensity[nr2] = fmaf(tx, ty, particleDensity[nr2]);

            if (touchState[nr3] != touch) {
                touchState[nr3] = touch;
                f[nr3] = 0.0f;
                w[nr3] = 0.0f;
                if (component == 0) particleDensity[nr3] = 0.0f;
            }
            f[nr3] = fmaf(pv, w3, f[nr3]); w[nr3] += w3;
            if (component == 0) particleDensity[nr3] = fmaf(sx, ty, particleDensity[nr3]);
        }

        for (uint16_t i = 0; i < CellCount; i++) {
            if ((touchState[i] == touch) && (w[i] > 0.0f)) {
                f[i] /= w[i];
            }
        }
        restore_boundary_velocity(component, f, prev);
    }
    update_rest_density();
}

void compute_grid_forces(uint16_t nIters) {
    memcpy(uPrev, uVel, sizeof(uVel));
    memcpy(vPrev, vVel, sizeof(vVel));

    /* 每帧一次：收集流体格，预计算 overRelaxiation/s */
    fluidCellCount = 0;
    for (uint16_t x = 1; x < CellNumX - 1U; x++) {
        for (uint16_t y = 1; y < CellNumY - 1U; y++) {
            uint16_t center = INDEX(x, y);
            if (CELL_TYPE(center) != FLUID_CELL) {
                continue;
            }
            float sx0 = (float)SOLID_MASK(INDEX(x - 1U, y));
            float sx1 = (float)SOLID_MASK(INDEX(x + 1U, y));
            float sy0 = (float)SOLID_MASK(INDEX(x, y - 1U));
            float sy1 = (float)SOLID_MASK(INDEX(x, y + 1U));
            float s = sx0 + sx1 + sy0 + sy1;
            if (s == 0.0f) {
                continue;
            }
            fluidCells[fluidCellCount++] = center;
            invSCoeff[center] = overRelaxiation / s;
            solidNeighbours[center] = (uint8_t)(SOLID_MASK(INDEX(x - 1U, y)) |
                                                (SOLID_MASK(INDEX(x + 1U, y)) << 1) |
                                                (SOLID_MASK(INDEX(x, y - 1U)) << 2) |
                                                (SOLID_MASK(INDEX(x, y + 1U)) << 3));
        }
    }

    for (uint16_t iter = 0; iter < nIters; iter++) {
        for (uint16_t k = 0; k < fluidCellCount; k++) {
            {
                uint16_t center = fluidCells[k];
                uint16_t x = center / CellNumY;
                uint16_t y = center % CellNumY;

                uint16_t right = INDEX(x + 1U, y);
                uint16_t top = INDEX(x, y + 1U);

                const uint32_t neighbours = solidNeighbours[center];
                float div = uVel[right] - uVel[center] + vVel[top] - vVel[center];

                if (particleRestDensity > 0.0f) {
                    float compression = particleDensity[center] - particleRestDensity;
                    if (compression > 0.0f) {
                        div -= compression;
                    }
                }

                const float p = -div * invSCoeff[center];

                uVel[center] -= (neighbours & 1U) * p;
                uVel[right] += ((neighbours >> 1) & 1U) * p;
                vVel[center] -= ((neighbours >> 2) & 1U) * p;
                vVel[top] += ((neighbours >> 3) & 1U) * p;
            }
        }
    }
}

void grid_to_particles(void) {
    const float flipRatio = 0.9f;

    for (int component = 0; component < 2; component++) {
        float *f = (component == 0) ? uVel : vVel;
        float *prevF = (component == 0) ? uPrev : vPrev;
        int offset = (component == 0) ? (int)CellNumY : 1;

        for (uint16_t i = 0; i < NumberOfParticles; i++) {
            int x0 = bilinX[component][i];
            int y0 = bilinY[component][i];
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float tx = bilin_q_to_float(bilinTx[component][i]);
            float ty = bilin_q_to_float(bilinTy[component][i]);
            float sx = 1.0f - tx;
            float sy = 1.0f - ty;

            float d0 = sx * sy;
            float d1 = tx * sy;
            float d2 = tx * ty;
            float d3 = sx * ty;

            uint16_t nr0 = INDEX((uint16_t)x0, (uint16_t)y0);
            uint16_t nr1 = INDEX((uint16_t)x1, (uint16_t)y0);
            uint16_t nr2 = INDEX((uint16_t)x1, (uint16_t)y1);
            uint16_t nr3 = INDEX((uint16_t)x0, (uint16_t)y1);

            float valid0 = (CELL_TYPE(nr0) != AIR_CELL || (int)nr0 - offset >= 0 && CELL_TYPE((uint16_t)((int)nr0 - offset)) != AIR_CELL) ? 1.0f : 0.0f;
            float valid1 = (CELL_TYPE(nr1) != AIR_CELL || (int)nr1 - offset >= 0 && CELL_TYPE((uint16_t)((int)nr1 - offset)) != AIR_CELL) ? 1.0f : 0.0f;
            float valid2 = (CELL_TYPE(nr2) != AIR_CELL || (int)nr2 - offset >= 0 && CELL_TYPE((uint16_t)((int)nr2 - offset)) != AIR_CELL) ? 1.0f : 0.0f;
            float valid3 = (CELL_TYPE(nr3) != AIR_CELL || (int)nr3 - offset >= 0 && CELL_TYPE((uint16_t)((int)nr3 - offset)) != AIR_CELL) ? 1.0f : 0.0f;

            float d = valid0 * d0 + valid1 * d1 + valid2 * d2 + valid3 * d3;
            if (d <= 0.0f) {
                continue;
            }

            float picV = (valid0 * d0 * f[nr0] + valid1 * d1 * f[nr1] + valid2 * d2 * f[nr2] + valid3 * d3 * f[nr3]) / d;
            float corr = (valid0 * d0 * (f[nr0] - prevF[nr0]) + valid1 * d1 * (f[nr1] - prevF[nr1])
                        + valid2 * d2 * (f[nr2] - prevF[nr2]) + valid3 * d3 * (f[nr3] - prevF[nr3])) / d;
            float oldV = particleVel[2 * i + (uint16_t)component];
            float flipV = oldV + corr;
            particleVel[2 * i + (uint16_t)component] = (1.0f - flipRatio) * picV + flipRatio * flipV;
        }
    }
}

void screen_update_unused() {
    //显示
    
    // 根据网格类型直接显示
    for (int y = 1; y < CellNumY-1; y++) {
        for (int x = 1; x < CellNumX-1; x++) {
            uint32_t cell = CELL_TYPE(INDEX(x, y));
            x--;
            y--;
            if (cell == FLUID_CELL) {
                LED_SetPixel(x*16+y,LEDON);
            } else if (cell == SOLID_CELL) {
                LED_SetPixel(x*16+y,LEDON);
            } else {
                LED_SetPixel(x*16+y,LEDOFF);
            }
            x++;
            y++;
            
        }
    }    
}
/* ---- 给主机验证和渲染用的访问器（原工程没有） ---- */
uint8_t sim_cell_type(uint16_t x, uint16_t y)
{
    return CELL_TYPE(INDEX(x, y));
}