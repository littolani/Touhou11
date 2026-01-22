#pragma once
#include "AnmVm.h"
#include "AnmLoaded.h"
#include "Chireiden.h"
#include "Chain.h"
#include "Macros.h"
#include "Vectors.h"

struct BoundingBox3
{
    D3DXVECTOR3 minPos;
    D3DXVECTOR3 maxPos;
};

struct Shooter
{
    uint8_t fireRate;
    uint8_t startDelay;
    uint16_t damage;
    D3DXVECTOR2 offset;
    D3DXVECTOR2 hitbox;
    float angle;
    float speed;
    uint8_t option;
    char kind;
    int16_t anmScript;
    int16_t anmScriptHit;
    int16_t sfx;
    void* onInit;
    void* onTick;
    void* onDraw;
    void* onHit;
};
ASSERT_SIZE(Shooter, 0x34);

struct PlayerBullet
{
    Timer timer0;
    D3DXVECTOR3 position;
    D3DXVECTOR3 velocity;
    float speed;
    float angle;
    float smth;
    float d_smth_dt;
    int idk[2];
    uint8_t flags;
    uint8_t probablyPadding[3];
    int someInt;
    AnmId anmId4c;
    AnmId anmId50;
    int idk4[3];
    int damage;
    int idk5;
    Shooter* shooter;
};

struct PlayerOption
{
    int idk[7];
    float targetAngleUnfocused;
    float targetDistanceUnfocused;
    float targetAngleFocused;
    float targetDistanceFocused;
    int idk2[7];
    AnmId anotherAnmId;
    int idk3[2];
    Int2 maybeTargetPosSubpixel;
    int idk4[2];
    Int2 int2_64;
    Int2 int2_6c;
    Int2 marisaBPreferredOffsetsByFormtion[5];
    int idk5[3];
    float angle;
    int idk6;
    AnmId anmIds[2];
    int idk7[6];
    int optionId;
    int resetFlagMaybe;
    int idk8;
    void* someCallback;
    void* someDrawCallback;
};
ASSERT_SIZE(PlayerOption, 0xe4);

struct PlayerDamageSource
{
    float argf0;
    float argf1;
    int idk[4];
    D3DXVECTOR3 centerPosition;
    int idk2[10];
    Timer timer;
    int argi1;
    int someInt;
    int someInt999999;
    int someInt6c;
    uint32_t flags;
};
ASSERT_SIZE(PlayerDamageSource, 0x74);

class Player
{
public:
    uint32_t idk;
    ChainElem* onTick;
    ChainElem* onDraw;
    ChainElem* chainElem2;
    AnmLoaded* playerAnm;
    AnmVm vm0;
    AnmVm vm1;
    D3DXVECTOR3 position;
    Int2 posSubpixel;
    int normalSpeedSubpixel;
    int focusSpeedSubpixel;
    int normalSpeedSubpixelOverSqrt2;
    int focusSpeedSubpixelOverSqrt2;
    D3DXVECTOR3 attemptedDeltaPosSubpixel;
    D3DXVECTOR3 lastNonzeroAttemptedDeltaPosSubpixel;
    Int2 attemptedDeltaPosISubpixel;
    int idk2;
    int idk3;
    int idk4;
    BoundingBox3 hurtbox;
    D3DXVECTOR3 hurtboxHalfSize;
    D3DXVECTOR3 itemAttractBoxUnfocusedHalfSize;
    D3DXVECTOR3 itemAttractBoxFocusedHalfSize;
    int idk5[3];
    Int2 attemptedVelocityInternal;
    int attemptedDirection;
    int reimuAGappingState;
    int reimuAFramesInGappingState;
    int state;
    uint32_t* shotFile;
    Timer timer0;
    Timer timer1;
    Timer timer2;
    PlayerBullet playerBullets[256];
    AnmId anmIdFocusedHitbox;
    PlayerOption playerOptions[8];
    int numberOfOptionsMaybe;
    uint32_t idk6;
    uint8_t idk7;
    uint8_t probablyPadding[3];
    PlayerDamageSource damageSouces[33];
    int idk8[4];
    int percentageMovedByOptions;
    int reimuB_FramesWithoutInput;
    int reimuC_FramesWithout_Z_Or_Shift;
    int marizaB_Formation;
    Timer timerIFrames;
    uint32_t someFlag;
    int someCounter;
    BoundingBox3 itemCollectBox;
    BoundingBox3 itemAttractBoxFocused;
    BoundingBox3 itemAttractBoxUnfocused;
    float reimuCOptionAngle;
    Int2 unused[33];
    int isFocused;

    static void setIframes(int currentTime);
};
ASSERT_SIZE(Player, 0x8d24);