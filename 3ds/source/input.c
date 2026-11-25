#include <3ds.h>
#include <string.h>
#include "input.h"
#include "protocol.h"

static bool is_new = false;
static int irrst_ok = 0;
static int gyro_on = 0;
static u32 held = 0, down = 0, prev = 0;

void input_init(void)
{
    APT_CheckNew3DS(&is_new);
    /* ZL/ZR/C-Stick chegam pelo servico ir:rst: nativos no New 3DS,
       via Circle Pad Pro no Old 3DS */
    irrst_ok = R_SUCCEEDED(irrstInit());
}

void input_exit(void)
{
    input_gyro(0);
    if (irrst_ok) irrstExit();
}

int input_is_new3ds(void) { return is_new; }

int input_has_cpp(void)
{
    if (is_new) return 1;
    return irrst_ok && (irrstKeysHeld() & (KEY_ZL | KEY_ZR | KEY_CSTICK_UP | KEY_CSTICK_DOWN | KEY_CSTICK_LEFT | KEY_CSTICK_RIGHT));
}

void input_scan(void)
{
    if (irrst_ok) irrstScanInput();
    held = hidKeysHeld();
    if (irrst_ok) held |= irrstKeysHeld();
    down = held & ~prev;
    prev = held;
}

uint32_t input_held(void) { return held; }
uint32_t input_down(void) { return down; }

void input_gyro(int on)
{
    if (on && !gyro_on) HIDUSER_EnableGyroscope();
    if (!on && gyro_on) HIDUSER_DisableGyroscope();
    gyro_on = on;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int axis(int v, int dz, int max)
{
    if (v > dz) v -= dz;
    else if (v < -dz) v += dz;
    else return 0;
    return clamp(v * 32767 / (max - dz), -32767, 32767);
}

void input_build(const ko_config_t *c, ko_pad_t *o)
{
    memset(o, 0, sizeof(*o));
    u32 k = held;

    /* face: posicional (o botao de baixo e sempre "A" do Xbox) ou por rotulo */
    if (c->layout == 0) {
        if (k & KEY_B) o->buttons |= KO_B_A;
        if (k & KEY_A) o->buttons |= KO_B_B;
        if (k & KEY_Y) o->buttons |= KO_B_X;
        if (k & KEY_X) o->buttons |= KO_B_Y;
    } else {
        if (k & KEY_A) o->buttons |= KO_B_A;
        if (k & KEY_B) o->buttons |= KO_B_B;
        if (k & KEY_X) o->buttons |= KO_B_X;
        if (k & KEY_Y) o->buttons |= KO_B_Y;
    }

    if (c->trig_mode == 0) {
        if (k & KEY_L) o->buttons |= KO_B_LB;
        if (k & KEY_R) o->buttons |= KO_B_RB;
        if (k & KEY_ZL) o->lt = 255;
        if (k & KEY_ZR) o->rt = 255;
    } else {
        if (k & KEY_L) o->lt = 255;
        if (k & KEY_R) o->rt = 255;
        if (k & KEY_ZL) o->buttons |= KO_B_LB;
        if (k & KEY_ZR) o->buttons |= KO_B_RB;
    }

    if (k & KEY_START)  o->buttons |= KO_B_START;
    if (k & KEY_SELECT) o->buttons |= KO_B_BACK;
    if (k & KEY_DUP)    o->buttons |= KO_B_UP;
    if (k & KEY_DDOWN)  o->buttons |= KO_B_DOWN;
    if (k & KEY_DLEFT)  o->buttons |= KO_B_LEFT;
    if (k & KEY_DRIGHT) o->buttons |= KO_B_RIGHT;

    circlePosition cp;
    hidCircleRead(&cp);
    o->lx = axis(cp.dx, c->deadzone, 150);
    o->ly = axis(cp.dy, c->deadzone, 150);

    circlePosition cs = { 0, 0 };
    if (irrst_ok) irrstCstickRead(&cs);
    o->rx = axis(cs.dx, 10, 146);
    o->ry = axis(cs.dy, 10, 146);

    if (gyro_on) {
        angularRate g;
        hidGyroRead(&g);
        /* libctru: x = roll, y = pitch, z = yaw. Alternativo: inclinar p/ os lados (roll) */
        int yaw = c->gyro_alt ? g.x : g.z;
        int pitch = g.y;
        if (yaw > -24 && yaw < 24) yaw = 0;
        if (pitch > -24 && pitch < 24) pitch = 0;
        int s = c->gyro_sens * 6;
        o->rx = clamp(o->rx - yaw * s, -32767, 32767);
        o->ry = clamp(o->ry + (c->gyro_invert_y ? -pitch : pitch) * s, -32767, 32767);
    }
}

int input_pack(const ko_pad_t *p, uint32_t seq, uint8_t *b)
{
    ko_header(b, KO_T_INPUT);
    uint8_t *q = b + KO_HDR_SIZE;
    ko_put32(q + 0, seq);
    ko_put32(q + 4, p->buttons);
    ko_put16(q + 8, (uint16_t)(int16_t)p->lx);
    ko_put16(q + 10, (uint16_t)(int16_t)p->ly);
    ko_put16(q + 12, (uint16_t)(int16_t)p->rx);
    ko_put16(q + 14, (uint16_t)(int16_t)p->ry);
    q[16] = (uint8_t)p->lt;
    q[17] = (uint8_t)p->rt;
    ko_put16(q + 18, (uint16_t)(int16_t)clamp(p->mdx, -32767, 32767));
    ko_put16(q + 20, (uint16_t)(int16_t)clamp(p->mdy, -32767, 32767));
    q[22] = (uint8_t)p->mbtn;
    q[23] = 0;
    ko_put16(q + 24, 0);
    q[26] = 0; q[27] = 0;
    return KO_HDR_SIZE + KO_INPUT_SIZE;
}
