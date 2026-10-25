/* CH32H417 双核 —— V5F（内容生成：2D 流体模拟）
 *
 * 64x64 网格、2500 个粒子的 PIC/FLIP 流体（SandSim.c）。
 * 每个粒子画成 4x4 像素，整块 128x128 居中放在 200x200 的墨水屏上。
 *
 * 本核只负责算和画，画完的内容进共享内存，V3F负责刷屏。

 */
#include <string.h>
#include <math.h>

#include "print.h"
#include "epd_fast.h"
#include "epd_share.h"
#include "SandSim.h"
#include "font_fixedsys.h"   /* Fixedsys 8x10 点阵 */
#include "fluidDemo.h"

#define GRID_PX   2                            /* 每个网格 2x2 像素 */
#define CELL_PX   4                            /* 每格画 4x4 像素：格子间距还是 2 像素，
                                                * 所以相邻格子重叠 2 像素，缝隙被完全封住 */
#define DRAW_W    ((int)CellNumX * GRID_PX)    /* 128 */
#define DRAW_H    ((int)CellNumY * GRID_PX)    /* 128 */
#define DRAW_X    ((EPD_W - DRAW_W) / 2)       /* 居中偏移 36 */
#define DRAW_Y    ((EPD_H - DRAW_H) / 2)

/* 底部左侧加速度箭头、右侧两行一位小数帧率。 */
#define FPS_CHARS  7                                /* V5:54.5 共 7 个字符 */
#define FPS_X      (EPD_W - FPS_CHARS * (FONT_W + 1) - 4)
#define FPS_Y1     (DRAW_Y + DRAW_H + 4)
#define FPS_Y2     (FPS_Y1 + FONT_H + 4)
#define ARROW_CX   24
#define ARROW_CY   ((FPS_Y1 + FPS_Y2 + FONT_H) / 2)

/* 假加速度序列 */
#define ACCEL_CYCLE_MS   (24u * 1000u)
#define ACCEL_A          6.8f

#define SIM_RUN_MS      120000u   /* 仿真时长*/

#define SIM_PUSH_ITER   2
#define SIM_GRID_ITER   14

static uint8_t frame[EPD_FRAME_BYTES] __attribute__((aligned(4)));

/* 描一个黑点（bit 0 = 黑） */
static void px(int x, int y)
{
    if ((x < 0) || (x >= EPD_W) || (y < 0) || (y >= EPD_H)) {
        return;
    }
    frame[(uint32_t)y * EPD_STRIDE + (uint32_t)(x >> 3)] &= (uint8_t)~(0x80u >> (x & 7));
}

/* ---------------------------------------------------------- 5x7 字体绘制 */
static void draw_char(int x, int y, char ch)
{
    const uint8_t *glyph;
    int j;

    if ((ch < FONT_FIRST) || (ch > FONT_LAST)) {
        return;
    }
    glyph = font_fixedsys[(int)ch - FONT_FIRST];

    for (j = 0; j < FONT_H; ++j) {
        const uint8_t row = glyph[j];
        int i;

        for (i = 0; i < FONT_W; ++i) {
            if ((row & (uint8_t)(1u << (FONT_W - 1 - i))) != 0u) {
                px(x + i, y + j);
            }
        }
    }
}

static void draw_text(int x, int y, const char *s)
{
    while (*s != 0) {
        draw_char(x, y, *s);
        ++s;
        x += FONT_W + 1;
    }
}

static int round_coord(float v)
{
    return (int)(v + ((v >= 0.0f) ? 0.5f : -0.5f));
}

static void px_thick(int x, int y)
{
    px(x, y);
    px(x + 1, y);
    px(x, y + 1);
    px(x + 1, y + 1);
}

static void draw_line(int x0, int y0, int x1, int y1)
{
    const int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    const int sx = (x0 < x1) ? 1 : -1;
    const int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        const int e2 = 2 * err;

        px_thick(x0, y0);
        if ((x0 == x1) && (y0 == y1)) {
            break;
        }
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_accel_arrow(float ax, float ay)
{
    const float magnitude = sqrtf(ax * ax + ay * ay);

    if (magnitude < 0.0001f) {
        px_thick(ARROW_CX, ARROW_CY);
        return;
    }

    float strength = magnitude / ACCEL_A;
    if (strength > 1.5f) {
        strength = 1.5f;
    }

    const float ux = ax / magnitude;
    const float uy = ay / magnitude;
    const float half_len = 4.0f + 5.0f * strength;
    const int x0 = round_coord((float)ARROW_CX - ux * half_len);
    const int y0 = round_coord((float)ARROW_CY - uy * half_len);
    const int xt = round_coord((float)ARROW_CX + ux * half_len);
    const int yt = round_coord((float)ARROW_CY + uy * half_len);
    const float head_len = 3.0f + 2.0f * strength;
    const float wing = 2.0f + strength;
    const int xh0 = round_coord((float)xt - ux * head_len - uy * wing);
    const int yh0 = round_coord((float)yt - uy * head_len + ux * wing);
    const int xh1 = round_coord((float)xt - ux * head_len + uy * wing);
    const int yh1 = round_coord((float)yt - uy * head_len - ux * wing);

    draw_line(x0, y0, xt, yt);
    draw_line(xt, yt, xh0, yh0);
    draw_line(xt, yt, xh1, yh1);
}

static void draw_rate(int x, int y, char core, uint32_t tenths)
{
    char buf[FPS_CHARS + 1];

    if (tenths > 999u) {
        tenths = 999u;
    }
    buf[0] = 'V';
    buf[1] = core;
    buf[2] = ':';
    buf[3] = (char)('0' + (int)(tenths / 100u));
    buf[4] = (char)('0' + (int)((tenths / 10u) % 10u));
    buf[5] = '.';
    buf[6] = (char)('0' + (int)(tenths % 10u));
    buf[7] = 0;
    draw_text(x, y, buf);
}

/* 逐粒子渲染：每个粒子画 4x4 像素（位置按仿真坐标换算）*/
static void render_particles(void)
{
    uint32_t i;

    memset(frame, 0xFF, sizeof(frame));

    for (i = 0; i < NumberOfParticles; ++i) {
        const float fx = particlePos[2u * i];
        const float fy = particlePos[2u * i + 1u];
        int cx = (int)(fx / Spacing);
        int cy = (int)(fy / Spacing);
        int px0;
        int py0;
        int dy;

        if (cx < 0) {
            cx = 0;
        }
        if (cx > (int)CellNumX - 1) {
            cx = (int)CellNumX - 1;
        }
        if (cy < 0) {
            cy = 0;
        }
        if (cy > (int)CellNumY - 1) {
            cy = (int)CellNumY - 1;
        }

        px0 = DRAW_X + cx * GRID_PX;
        py0 = DRAW_Y + cy * GRID_PX;

        for (dy = 0; dy < CELL_PX; ++dy) {
            int dx;

            for (dx = 0; dx < CELL_PX; ++dx) {
                px(px0 + dx, py0 + dy);
            }
        }
    }
}

void fluidDemo(void)
{
    uint32_t n = 0;
    uint32_t t0;
    uint32_t sim_fps_x10 = 0;               /* V5F 自己的仿真帧率 */
    uint32_t fps_n = 0;                 /* 上次统计时的帧计数 */
    uint32_t fps_ms = 0;                /* 上次统计的时刻（毫秒） */

    SystemAndCoreClockUpdate();
    Delay_Init();
    PRINT_INIT();                       /* 走 IPC，由 V3F 的 USART1 打出来 */
    epd_share_init();
    EPD_TickInit();                      //初始化时基。其实只是给V5F的demo用，驱动用不到这个。

    InitParticles();                    
    t0 = EPD_Millis();
    fps_ms = t0;

    /* 注意：newlib-nano 的 printf 不带浮点，这里全部用整数 */
    PRINT("[V5F] fluid: %ux%u cells, %u particles, %dx%d px\r\n",
          (unsigned)CellNumX, (unsigned)CellNumY,
          (unsigned)NumberOfParticles, (int)DRAW_W, (int)DRAW_H);

    while ((uint32_t)(EPD_Millis() - t0) < SIM_RUN_MS) {
        /* 假加速度（板子没有加速度计）：用真实时钟驱动一段演示序列
         * 然后 12 秒循环。幅值取 9.8，和重力一样。 */
        const uint32_t ms = (uint32_t)(EPD_Millis() - t0);
        const uint32_t cyc = ms % ACCEL_CYCLE_MS;
        float ax;
        float ay;

        if (cyc < 8u * 1000u) {
            ax = -ACCEL_A;                  /* 向左 */
            ay = 0.0f;
        } else if (cyc < 10u * 1000u) {
            ax = ACCEL_A;                   /* 向右 */
            ay = 0.0f;
        } else if (cyc < 12u * 1000u) {
            ax = 0.0f;
            ay = -ACCEL_A;                  /* 向上（y 向下为正） */
        } else if (cyc < 14u * 1000u) {
            ax = 0.0f;
            ay = ACCEL_A;                   /* 向下 */
        } else {
            const float ang = (float)(cyc - 4u * 1000u) *
                              (6.2831853f / (float)8000u);

            ax = ACCEL_A * (float)sin((double)ang);
            ay = ACCEL_A * (float)cos((double)ang);
        }

        //流体模拟的主循环。
        ParticleIntegrate(ax, ay);
        PushParticlesApart(SIM_PUSH_ITER);
        particles_to_grid();
        density_update();
        compute_grid_forces(SIM_GRID_ITER);
        grid_to_particles();

        //绘制仿真结果
        render_particles();
        draw_accel_arrow(ax, ay);
        draw_rate(FPS_X, FPS_Y1, '5', sim_fps_x10);
        draw_rate(FPS_X, FPS_Y2, '3', EPD_SHARE->cons_fps);

        EPD_FastFrame(frame);           //发布到共享内存
        Delay_Ms(7);//等一会，不跑太快。

        /* 统计自己的仿真帧率：每满 1 秒算一次 */
        ++n;
        if ((n % 45u) == 0u) {
            const uint32_t now_ms = (uint32_t)(EPD_Millis() - t0);
            const uint32_t dt_ms = now_ms - fps_ms;   /* 注意：dt 是 SandSim.h 的宏，不能重名 */

            if (dt_ms != 0u) {
                sim_fps_x10 = (n - fps_n) * 10000u / dt_ms;
            }
            EPD_SHARE->prod_fps = sim_fps_x10;
            fps_n = n;
            fps_ms = now_ms;
            PRINT("[V5F] %u frames, sim %u fps\r\n", (unsigned)n, (unsigned)(sim_fps_x10 / 10u));
        }
    }

    //从循环出来就是仿真结束了，V5核回去睡大觉
    PRINT("[V5F] sim stopped after %u s, %u frames\r\n",
          (unsigned)(SIM_RUN_MS / 1000u), (unsigned)n);
    
    for (;;) {
        __WFI();
    }
}
