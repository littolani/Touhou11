#pragma once
#include "AnmVm.h"
#include "AnmLoaded.h"
#include "Chireiden.h"
#include "Chain.h"
#include "Macros.h"
#include "Shottypes.h"
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
    int isActive;
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
    int idk7[4];
    D3DXVECTOR2 someOtherFloat;
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
    uint32_t idk;                                          // <0x0>
    ChainElem* onTick;                                     // <0x4>
    ChainElem* onDraw;                                     // <0x8>
    ChainElem* chainElem2;                                 // <0xc>
    AnmLoaded* playerAnm;                                  // <0x10>
    AnmVm vm0;                                             // <0x14>
    AnmVm vm1;                                             // <0x448>
    D3DXVECTOR3 position;                                  // <0x87c>
    Int2 posSubpixel;                                      // <0x888>
    int normalSpeedSubpixel;                               // <0x990>
    int focusSpeedSubpixel;                                // <0x894>
    int normalSpeedSubpixelOverSqrt2;                      // <0x898>
    int focusSpeedSubpixelOverSqrt2;                       // <0x89c>
    D3DXVECTOR3 attemptedDeltaPosSubpixel;                 // <0x8a0>
    D3DXVECTOR3 lastNonzeroAttemptedDeltaPosSubpixel;      // <0x8ac>
    Int2 attemptedDeltaPosISubpixel;                       // <0x8b8>
    int idk2;                                              // <0x8c0>
    int idk3;                                              // <0x8c4>
    int idk4;                                              // <0x8c8>
    BoundingBox3 hurtbox;                                  // <0x8cc>
    D3DXVECTOR3 hurtboxHalfSize;                           // <0x8e4>
    D3DXVECTOR3 itemAttractBoxUnfocusedHalfSize;           // <0x8f0>
    D3DXVECTOR3 itemAttractBoxFocusedHalfSize;             // <0x8fc>
    int idk5[3];                                           // <0x908>
    Int2 attemptedVelocityInternal;                        // <0x914>
    int attemptedDirection;                                // <0x91c>
    int reimuAGappingState;                                // <0x920>
    int reimuAFramesInGappingState;                        // <0x924>
    int state;                                             // <0x928>
    uint32_t* shotFile;                                    // <0x92c>
    Timer timer0;                                          // <0x930>
    Timer timer1;                                          // <0x944>
    Timer timer2;                                          // <0x958>
    PlayerBullet playerBullets[255];                       // <0x96c>
    AnmId anmIdFocusedHitbox;                              // <0x7500>
    PlayerOption playerOptions[8];                         // <0x7504>
    int idk6[30];                                          // <0x7c24>
    PlayerDamageSource damageSouces[33];                   // <0x7c9c>
    int shooterOptions[4];                                 // <0x8b90>
    int percentageMovedByOptions;                          // <0x8ba0>
    int reimuB_FramesWithoutInput;                         // <0x8ba4>
    int reimuC_FramesWithout_Z_Or_Shift;                   // <0x8ba8>
    int marisaB_Formation;                                 // <0x8bac>
    Timer timerIFrames;                                    // <0x8bb0>
    uint32_t someFlag;                                     // <0x8bc4>
    int someCounter;                                       // <0x8bc8>
    BoundingBox3 itemCollectBox;                           // <0x8bcc>
    BoundingBox3 itemAttractBoxFocused;                    // <0x8be4>
    BoundingBox3 itemAttractBoxUnfocused;                  // <0x8bfc>
    float reimuCOptionAngle;                               // <0x8c14>
    Int2 unused[33];                                       // <0x8c18>
    int isFocused;                                         // <0x8c20>

    // 0x433a90
    static PlayerDamageSource* createDamageSource(Player* This, D3DXVECTOR3* x, float argF0, float argF1, int currentTime, int argi1);

    // 0x4327d0
    static void idk0(Player* This);

    // 0x42f8a0
    static int initialize(Player* This);

    // 0x431c70
    static int loadShotFile(Player* This, char* filename);

    // 0x4336a0
    static void optionCallbackReimuA();

    // 0x4337b0
    static void optionCallbackReimuC(Player* This);

    // 0x430290
    static int move(Player* This);

    // 0x42f760 
    // Player();

    // 0x430e50
    static void reimuATickGappingState(Player* This);

    // 0x430270
    static void release(Player* This);

    // 0x432cc0
    static void repopulateOptions(Player* This);

    // 0x410610
    static void resetOptions(Player* This);

    // 0x410610
    static void setIframes(int currentTime);

    // 0x410610
    static void shoot(Player* This, int currentTime);

    // 0x434380
    static int shootingTick(Player* This, /*EnemyManager* enemyManager,*/ int someNumber);

    // 0x433f90
    static int shootOneBullet(Player* This, D3DXVECTOR3* position, int currentTime, Shooter* shooter);

    // 0x434440
    static int tickBullets(Player* This);

    // 0x432a90
    static void timerRelated(Player* This, uint8_t di);

    // 0x432a90
    static void useBomb();
};
ASSERT_SIZE(Player, 0x8d24);