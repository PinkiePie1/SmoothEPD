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
/* nano libc 的 memset/memcpy 是逐字节循环。这里换成按字拷贝/清零。 */
typedef unsigned int alias_word __attribute__((may_alias));

static void zero_words(void *p, unsigned int bytes)
{
    unsigned char *b = (unsigned char *)p;
    alias_word *w;

    while ((bytes != 0U) && (((uintptr_t)b & 3U) != 0U)) {
        *b++ = 0U;
        --bytes;
    }
    w = (alias_word *)b;
    while (bytes >= 4U) {
        *w++ = 0U;
        bytes -= 4U;
    }
    b = (unsigned char *)w;
    while (bytes-- != 0U) {
        *b++ = 0U;
    }
}

/*
static void copy_words(void *dst, const void *src, unsigned int bytes)
{
    unsigned char *db = (unsigned char *)dst;
    const unsigned char *sb = (const unsigned char *)src;
    alias_word *d;
    const alias_word *s;

    while ((bytes != 0U) &&
           ((((uintptr_t)db | (uintptr_t)sb) & 3U) != 0U)) {
        *db++ = *sb++;
        --bytes;
    }
    d = (alias_word *)db;
    s = (const alias_word *)sb;
    while (bytes >= 4U) {
        *d++ = *s++;
        bytes -= 4U;
    }
    db = (unsigned char *)d;
    sb = (const unsigned char *)s;
    while (bytes-- != 0U) {
        *db++ = *sb++;
    }
}
*/

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
static float uPrev[CellCount];
static float vPrev[CellCount];
static float uWeight[CellCount];
static float vWeight[CellCount];

static float particleDensity[CellCount];
static uint8_t solidMask[CellCount];
static uint8_t cellType[CellCount];
static float particleRestDensity = 0.0f;
/* 压力求解用：流体格索引表 + 每格预计算的 overRelaxiation/s 系数。
 * s 只跟 cellType/solidMask 有关，每帧算一次即可，迭代里不再做除法。 */
static uint16_t fluidCells[CellCount];
/* 原来这里还有一张 float invSCoeff[CellCount] 缓存（overRelaxiation/s）。
 * 64x64 网格下它要 16 KB，V5F 的 RAM 放不下，改成压力迭代里现算一次除法
 * （有硬件 FPU，一次除法很便宜）。收集流体格时已保证 s != 0。 */
static uint16_t fluidCellCount;

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
    for (uint16_t x = 0; x < CellNumX; x++) {
        for (uint16_t y = 0; y < CellNumY; y++) {
            uint8_t s = 1;
            if (x == 0U || x == CellNumX - 1U || y == 0U || y == CellNumY - 1U) {
                s = 0;
            }
            solidMask[INDEX(x, y)] = s;
        }
    }
}

void InitParticles() {
    zero_words(particleVel, sizeof(particleVel));
    zero_words(uVel, sizeof(uVel));
    zero_words(vVel, sizeof(vVel));
    zero_words(uPrev, sizeof(uPrev));
    zero_words(vPrev, sizeof(vPrev));
    zero_words(particleDensity, sizeof(particleDensity));
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
    zero_words(cellParticleCountPrefix, sizeof(cellParticleCountPrefix));
    zero_words(particlePosId, sizeof(particlePosId));

    // --- build grid ---
    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        float px = particlePos[XID(i)];
        float py = particlePos[YID(i)];

        uint16_t xi = (uint16_t)clamp_index((int)(px * invertSpacing), 0, (int)CellNumX - 1);
        uint16_t yi = (uint16_t)clamp_index((int)(py * invertSpacing), 0, (int)CellNumY - 1);

        cellParticleCountPrefix[INDEX(xi, yi)]++;
    }

    uint16_t prefix = 0;
    for (uint16_t i = 0; i < CellCount; i++) {
        prefix += cellParticleCountPrefix[i];
        cellParticleCountPrefix[i] = prefix;
    }
    cellParticleCountPrefix[CellCount] = prefix;

    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        float px = particlePos[XID(i)];
        float py = particlePos[YID(i)];

        uint16_t xi = (uint16_t)clamp_index((int)(px * invertSpacing), 0, (int)CellNumX - 1);
        uint16_t yi = (uint16_t)clamp_index((int)(py * invertSpacing), 0, (int)CellNumY - 1);

        uint16_t cellNr = INDEX(xi, yi);
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


void density_update(void) {
    zero_words(particleDensity, sizeof(particleDensity));

    const float h = Spacing;
    const float h1 = invertSpacing;
    const float h2 = 0.5f * h;

    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        float x = clampf_local(particlePos[XID(i)], h, (CellNumX - 1U) * h);
        float y = clampf_local(particlePos[YID(i)], h, (CellNumY - 1U) * h);

        int x0 = floor_to_int_nonneg((x - h2) * h1);
        int y0 = floor_to_int_nonneg((y - h2) * h1);
        x0 = clamp_index(x0, 0, (int)CellNumX - 2);
        y0 = clamp_index(y0, 0, (int)CellNumY - 2);
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        float tx = ((x - h2) - x0 * h) * h1;
        float ty = ((y - h2) - y0 * h) * h1;
        float sx = 1.0f - tx;
        float sy = 1.0f - ty;

        particleDensity[INDEX(x0, y0)] += sx * sy;
        particleDensity[INDEX(x1, y0)] += tx * sy;
        particleDensity[INDEX(x1, y1)] += tx * ty;
        particleDensity[INDEX(x0, y1)] += sx * ty;
    }

    if (particleRestDensity == 0.0f) {
        float sum = 0.0f;
        uint16_t numFluid = 0;
        for (uint16_t i = 0; i < CellCount; i++) {
            if (cellType[i] == FLUID_CELL) {
                sum += particleDensity[i];
                numFluid++;
            }
        }
        if (numFluid > 0U) {
            particleRestDensity = sum / (float)numFluid;
        }
    }
}

/* 双线性采样几何缓存：p2g（两个分量）和 g2p（两个分量）用的是完全相同的
 * x0/y0/tx/ty，每帧只算一次即可（原本算 4 遍，density 另有自己的一套）。 */
typedef struct {
    uint8_t x0, y0;
    float tx, ty;
} BilinGeom;

static BilinGeom bilinGeom[2][NumberOfParticles];

static void build_bilinear_geom(void)
{
    const float h  = Spacing;
    const float h1 = invertSpacing;
    const float h2 = 0.5f * h;

    for (int c = 0; c < 2; c++) {
        float dx = (c == 0) ? 0.0f : h2;
        float dy = (c == 0) ? h2 : 0.0f;
        for (uint16_t i = 0; i < NumberOfParticles; i++) {
            float x = clampf_local(particlePos[XID(i)], h, (CellNumX - 1U) * h);
            float y = clampf_local(particlePos[YID(i)], h, (CellNumY - 1U) * h);
            int x0 = clamp_index(floor_to_int_nonneg((x - dx) * h1), 0, (int)CellNumX - 2);
            int y0 = clamp_index(floor_to_int_nonneg((y - dy) * h1), 0, (int)CellNumY - 2);
            bilinGeom[c][i].x0 = (uint8_t)x0;
            bilinGeom[c][i].y0 = (uint8_t)y0;
            bilinGeom[c][i].tx = ((x - dx) - x0 * h) * h1;
            bilinGeom[c][i].ty = ((y - dy) - y0 * h) * h1;
        }
    }
}

void particles_to_grid(void) {
    build_bilinear_geom();   /* 本帧的采样几何只算一次，p2g/g2p 共用 */
    memcpy(uPrev, uVel, sizeof(uVel));
    memcpy(vPrev, vVel, sizeof(vVel));

    zero_words(uVel, sizeof(uVel));
    zero_words(vVel, sizeof(vVel));
    zero_words(uWeight, sizeof(uWeight));
    zero_words(vWeight, sizeof(vWeight));

    for (uint16_t i = 0; i < CellCount; i++) {
        cellType[i] = (solidMask[i] == 0) ? SOLID_CELL : AIR_CELL;
    }

    for (uint16_t i = 0; i < NumberOfParticles; i++) {
        int xi = clamp_index(floor_to_int_nonneg(particlePos[XID(i)] * invertSpacing), 0, (int)CellNumX - 1);
        int yi = clamp_index(floor_to_int_nonneg(particlePos[YID(i)] * invertSpacing), 0, (int)CellNumY - 1);
        uint16_t cellNr = INDEX((uint16_t)xi, (uint16_t)yi);
        if (cellType[cellNr] == AIR_CELL) {
            cellType[cellNr] = FLUID_CELL;
        }
    }

//    const float h = Spacing;
//    const float h1 = invertSpacing;
//    const float h2 = 0.5f * h;

    for (int component = 0; component < 2; component++) {
//        float dx = (component == 0) ? 0.0f : h2;
 //       float dy = (component == 0) ? h2 : 0.0f;
        float *f = (component == 0) ? uVel : vVel;
        float *w = (component == 0) ? uWeight : vWeight;

        const BilinGeom *bg = bilinGeom[component];

        for (uint16_t i = 0; i < NumberOfParticles; i++) {
            int x0 = bg[i].x0;
            int y0 = bg[i].y0;
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float tx = bg[i].tx;
            float ty = bg[i].ty;
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

            f[nr0] += pv * w0; w[nr0] += w0;
            f[nr1] += pv * w1; w[nr1] += w1;
            f[nr2] += pv * w2; w[nr2] += w2;
            f[nr3] += pv * w3; w[nr3] += w3;
        }

        for (uint16_t i = 0; i < CellCount; i++) {
            if (w[i] > 0.0f) {
                f[i] /= w[i];
            }
        }

        for (uint16_t x = 0; x < CellNumX; x++) {
            for (uint16_t y = 0; y < CellNumY; y++) {
                uint16_t idx = INDEX(x, y);
                uint16_t solid = (cellType[idx] == SOLID_CELL) ? 1U : 0U;
                if (component == 0) {
                    uint16_t leftSolid = (x > 0U && cellType[INDEX(x - 1U, y)] == SOLID_CELL) ? 1U : 0U;
                    if (solid || leftSolid) {
                        uVel[idx] = uPrev[idx];
                    }
                } else {
                    uint16_t bottomSolid = (y > 0U && cellType[INDEX(x, y - 1U)] == SOLID_CELL) ? 1U : 0U;
                    if (solid || bottomSolid) {
                        vVel[idx] = vPrev[idx];
                    }
                }
            }
        }
    }
}

void compute_grid_forces(uint16_t nIters) {
    memcpy(uPrev, uVel, sizeof(uVel));
    memcpy(vPrev, vVel, sizeof(vVel));

    /* 每帧一次：收集流体格，预计算 overRelaxiation/s */
    fluidCellCount = 0;
    for (uint16_t x = 1; x < CellNumX - 1U; x++) {
        for (uint16_t y = 1; y < CellNumY - 1U; y++) {
            uint16_t center = INDEX(x, y);
            if (cellType[center] != FLUID_CELL) {
                continue;
            }
            float sx0 = (float)solidMask[INDEX(x - 1U, y)];
            float sx1 = (float)solidMask[INDEX(x + 1U, y)];
            float sy0 = (float)solidMask[INDEX(x, y - 1U)];
            float sy1 = (float)solidMask[INDEX(x, y + 1U)];
            float s = sx0 + sx1 + sy0 + sy1;
            if (s == 0.0f) {
                continue;
            }
            fluidCells[fluidCellCount++] = center;
        }
    }

    for (uint16_t iter = 0; iter < nIters; iter++) {
        for (uint16_t k = 0; k < fluidCellCount; k++) {
            {
                uint16_t center = fluidCells[k];
                uint16_t x = center / CellNumY;
                uint16_t y = center % CellNumY;

                uint16_t left = INDEX(x - 1U, y);
                uint16_t right = INDEX(x + 1U, y);
                uint16_t bottom = INDEX(x, y - 1U);
                uint16_t top = INDEX(x, y + 1U);

                float sx0 = (float)solidMask[left];
                float sx1 = (float)solidMask[right];
                float sy0 = (float)solidMask[bottom];
                float sy1 = (float)solidMask[top];

                float div = uVel[right] - uVel[center] + vVel[top] - vVel[center];

                if (particleRestDensity > 0.0f) {
                    float compression = particleDensity[center] - particleRestDensity;
                    if (compression > 0.0f) {
                        div -= compression;
                    }
                }

                /* 现算 overRelaxiation/s：原来存在 invSCoeff 里（16 KB），
                 * V5F 内存放不下，这里用硬件 FPU 直接除。 */
                float s = sx0 + sx1 + sy0 + sy1;
                float p = -div * (overRelaxiation / s);

                uVel[center] -= sx0 * p;
                uVel[right] += sx1 * p;
                vVel[center] -= sy0 * p;
                vVel[top] += sy1 * p;
            }
        }
    }
}

void grid_to_particles(void) {
    const float flipRatio = 0.9f;
 //   const float h = Spacing;
 //   const float h1 = invertSpacing;
//    const float h2 = 0.5f * h;

    for (int component = 0; component < 2; component++) {
 //       float dx = (component == 0) ? 0.0f : h2;
 //       float dy = (component == 0) ? h2 : 0.0f;
        float *f = (component == 0) ? uVel : vVel;
        float *prevF = (component == 0) ? uPrev : vPrev;
        int offset = (component == 0) ? (int)CellNumY : 1;

        const BilinGeom *bg = bilinGeom[component];

        for (uint16_t i = 0; i < NumberOfParticles; i++) {
            int x0 = bg[i].x0;
            int y0 = bg[i].y0;
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float tx = bg[i].tx;
            float ty = bg[i].ty;
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

            float valid0 = (cellType[nr0] != AIR_CELL || (int)nr0 - offset >= 0 && cellType[(uint16_t)((int)nr0 - offset)] != AIR_CELL) ? 1.0f : 0.0f;
            float valid1 = (cellType[nr1] != AIR_CELL || (int)nr1 - offset >= 0 && cellType[(uint16_t)((int)nr1 - offset)] != AIR_CELL) ? 1.0f : 0.0f;
            float valid2 = (cellType[nr2] != AIR_CELL || (int)nr2 - offset >= 0 && cellType[(uint16_t)((int)nr2 - offset)] != AIR_CELL) ? 1.0f : 0.0f;
            float valid3 = (cellType[nr3] != AIR_CELL || (int)nr3 - offset >= 0 && cellType[(uint16_t)((int)nr3 - offset)] != AIR_CELL) ? 1.0f : 0.0f;

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
            uint32_t cell = cellType[INDEX(x, y)];
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
    return cellType[INDEX(x, y)];
}