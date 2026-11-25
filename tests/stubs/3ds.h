/* Stub minimo da libctru, so para checar o codigo do 3DS com o gcc do PC. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64;
typedef s32 Result;
#define R_FAILED(r) ((r) < 0)
#define R_SUCCEEDED(r) ((r) >= 0)
#define BIT(n) (1U << (n))

typedef enum { GSP_RGBA8_OES = 0, GSP_BGR8_OES, GSP_RGB565_OES, GSP_RGB5_A1_OES, GSP_RGBA4_OES } GSPGPU_FramebufferFormat;
typedef enum { GFX_TOP = 0, GFX_BOTTOM = 1 } gfxScreen_t;
typedef enum { GFX_LEFT = 0, GFX_RIGHT = 1 } gfx3dSide_t;
void gfxInit(GSPGPU_FramebufferFormat topFormat, GSPGPU_FramebufferFormat bottomFormat, bool vrambuffers);
void gfxExit(void);
void gfxSetDoubleBuffering(gfxScreen_t screen, bool enable);
u8* gfxGetFramebuffer(gfxScreen_t screen, gfx3dSide_t side, u16* width, u16* height);
void gfxScreenSwapBuffers(gfxScreen_t scr, bool hasStereo);
void gfxFlushBuffers(void);
void gspWaitForVBlank(void);

enum {
    KEY_A = BIT(0), KEY_B = BIT(1), KEY_SELECT = BIT(2), KEY_START = BIT(3), KEY_DRIGHT = BIT(4),
    KEY_DLEFT = BIT(5), KEY_DUP = BIT(6), KEY_DDOWN = BIT(7), KEY_R = BIT(8), KEY_L = BIT(9),
    KEY_X = BIT(10), KEY_Y = BIT(11), KEY_ZL = BIT(14), KEY_ZR = BIT(15), KEY_TOUCH = BIT(20),
    KEY_CSTICK_RIGHT = BIT(24), KEY_CSTICK_LEFT = BIT(25), KEY_CSTICK_UP = BIT(26), KEY_CSTICK_DOWN = BIT(27),
    KEY_CPAD_RIGHT = BIT(28), KEY_CPAD_LEFT = BIT(29), KEY_CPAD_UP = BIT(30), KEY_CPAD_DOWN = BIT(31),
};
typedef struct { s16 dx, dy; } circlePosition;
typedef struct { u16 px, py; } touchPosition;
typedef struct { s16 x; s16 z; s16 y; } angularRate;
void hidScanInput(void);
u32 hidKeysHeld(void);
void hidCircleRead(circlePosition* pos);
void hidTouchRead(touchPosition* pos);
void hidGyroRead(angularRate* rate);
Result HIDUSER_EnableGyroscope(void);
Result HIDUSER_DisableGyroscope(void);
Result irrstInit(void);
void irrstExit(void);
void irrstScanInput(void);
u32 irrstKeysHeld(void);
void irrstCstickRead(circlePosition* pos);

bool aptMainLoop(void);
Result APT_CheckNew3DS(bool* out);
void osSetSpeedupEnable(bool enable);
u64 osGetTime(void);
u8 osGetWifiStrength(void);
u64 svcGetSystemTick(void);
void svcSleepThread(s64 ns);
#define CPU_TICKS_PER_MSEC 268111.856
Result acInit(void);
void acExit(void);
Result ACU_GetWifiStatus(u32 *out);
Result socInit(u32* context_addr, u32 context_size);
Result socExit(void);
void* linearAlloc(size_t size);
void linearFree(void* mem);
Result DSP_FlushDataCache(const void* address, u32 size);

typedef enum { NDSP_OUTPUT_MONO = 0, NDSP_OUTPUT_STEREO = 1, NDSP_OUTPUT_SURROUND = 2 } ndspOutputMode;
typedef enum { NDSP_INTERP_POLYPHASE = 0, NDSP_INTERP_LINEAR = 1, NDSP_INTERP_NONE = 2 } ndspInterpType;
enum { NDSP_WBUF_FREE = 0, NDSP_WBUF_QUEUED = 1, NDSP_WBUF_PLAYING = 2, NDSP_WBUF_DONE = 3 };
#define NDSP_FORMAT_MONO_PCM16 5
typedef struct ndspWaveBuf { union { s8* data_pcm8; s16* data_pcm16; u8* data_adpcm; const void* data_vaddr; }; u32 nsamples; void* adpcm_data; u32 offset; bool looping; u8 status; u16 sequence_id; struct ndspWaveBuf* next; } ndspWaveBuf;
Result ndspInit(void);
void ndspExit(void);
void ndspSetOutputMode(ndspOutputMode mode);
void ndspChnReset(int id);
void ndspChnSetInterp(int id, ndspInterpType type);
void ndspChnSetRate(int id, float rate);
void ndspChnSetFormat(int id, u16 format);
void ndspChnSetMix(int id, float mix[12]);
void ndspChnWaveBufAdd(int id, ndspWaveBuf* buf);
void ndspChnWaveBufClear(int id);

typedef enum { SWKBD_TYPE_NORMAL = 0, SWKBD_TYPE_QWERTY, SWKBD_TYPE_NUMPAD, SWKBD_TYPE_WESTERN } SwkbdType;
typedef enum { SWKBD_ANYTHING = 0, SWKBD_NOTEMPTY } SwkbdValidInput;
typedef enum { SWKBD_BUTTON_LEFT = 0, SWKBD_BUTTON_MIDDLE, SWKBD_BUTTON_RIGHT, SWKBD_BUTTON_CONFIRM = SWKBD_BUTTON_RIGHT, SWKBD_BUTTON_NONE } SwkbdButton;
typedef struct { int dummy[256]; } SwkbdState;
void swkbdInit(SwkbdState* swkbd, SwkbdType type, int numButtons, int maxTextLength);
void swkbdSetButton(SwkbdState* swkbd, SwkbdButton button, const char* text, bool submit);
void swkbdSetHintText(SwkbdState* swkbd, const char* text);
void swkbdSetInitialText(SwkbdState* swkbd, const char* text);
void swkbdSetNumpadKeys(SwkbdState* swkbd, int left, int right);
void swkbdSetValidation(SwkbdState* swkbd, SwkbdValidInput validInput, u32 filterFlags, int maxDigits);
SwkbdButton swkbdInputText(SwkbdState* swkbd, char* buf, size_t bufsize);
