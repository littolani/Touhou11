#pragma once
#include "Chireiden.h"
#include "Macros.h"

struct GameConfig
{
    uint32_t version;          // <0x00> always 0x110003 for a valid file
    uint16_t shootKey;         // <0x04> shot (Z)
    uint16_t bombKey;          // <0x06> bomb (X)
    uint16_t focusKey;         // <0x08> focus / slow (Shift)
    uint16_t pauseKey;         // <0x0A> pause / menu (Esc)
    uint16_t upKey;            // <0x0C> up (Up-Arrow)
    uint16_t downKey;          // <0x0E> down (Down-Arrow)
    uint16_t leftKey;          // <0x10> left (Left-Arrow)
    uint16_t rightKey;         // <0x12> right (Right-Arrow)
    uint16_t refreshRate;      // <0x14> monitor refresh rate (forced to 60)
    uint16_t padDeadzoneX;     // <0x16> pad X dead-zone / sensitivity (0-1000, default 600)
    uint16_t padDeadzoneY;     // <0x18> pad Y dead-zone / sensitivity (same as X)
    uint8_t  colorDepth;       // <0x1A> 0 = 32-bit (recommended), 1 = 16-bit
    uint8_t  sfxEnabled;       // <0x1B> 1 = SE on (forced)
    uint8_t  startingBombs;    // <0x1C> 0 or 1 (validated < 2)
    uint8_t  displayMode;      // <0x1D> 0 = fullscreen 640×480
                               //       1 = window 640×480
                               //       2 = window 960×720
                               //       3 = window 1280×960
    uint8_t  frameSkip;        // <0x1E> 0 = every frame (60 FPS)
                               //       1 = 1/2 (approximately 30 FPS)
                               //       2 = 1/3 (approximately 20 FPS)
    uint8_t  musicMode;        // <0x1F> 0 = off, 1 = MIDI, 2 = WAV (validated < 3)
    uint8_t  bgmVolume;        // <0x20> 0-100 (default 100)
    uint8_t  sfxVolume;        // <0x21> 0-100 (default 80)
    uint8_t  isWindowed;       // <0x22> 0 = fullscreen, 1 = windowed (derived from displayMode)
    uint8_t  latencyMode;      // <0x23> 0 = stable, 1 = normal, 2 = auto (recommended), 3 = fast
    uint16_t padXaxis;         // <0x24> unknown pad X axis field
    uint16_t padYaxis;         // <0x26> unknown pad Y axis field
    uint32_t windowPosX;       // <0x28> 0x80000000 = centered
    uint32_t windowPosY;       // <0x2C> 0x80000000 = centered
    uint32_t unused1;          // <0x30> (no log, no UI)
    uint32_t unused2;          // <0x34> (no log, no UI)

    // --------------------------------------------------------------------
    // 0x38 : flags bitfield (default 0x100 = bit 8 set)
    // --------------------------------------------------------------------
    uint32_t flags;
    //  bit 0 (0x0001) :  Use 16-bit textures                      (16Bit のテクスチャの使用を強制します)
    //  bit 1 (0x0002) :  Force reference rasterizer               (リファレンスラスタライザを強制します)
    //  bit 2 (0x0004) :  Disable fog                              (フォグの使用を抑制します)
    //  bit 3 (0x0008) :  Do not use DirectInput for pad/keyboard  (パッド、キーボードの入力に DirectInput を使用しません)
    //  bit 4 (0x0010) :  Load BGM into memory                     (ＢＧＭをメモリに読み込みます)
    //  bit 5 (0x0020) :  Disable V-Sync                           (垂直同期を取りません)
    //  bit 6 (0x0040) :  Do not auto-detect text rendering env    (文字描画の環境を自動検出しません)
    //  bit 7 (0x0080) :  Unused/Unknown
    //  bit 8 (0x0100) :  Ask every time on startup
    //  bit 9 (0x0200) :  Slow movement while holding shot button
    //  bits 10-31     :  Unused
    // --------------------------------------------------------------------

    // Constructor
    GameConfig();
};
ASSERT_SIZE(GameConfig, 0x3C);
extern GameConfig g_defaultGameConfig;