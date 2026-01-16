#pragma once
#include "Chireiden.h"
#include "AnmVm.h"
#include "Interp.h"
#include "Macros.h"
#include "Timer.h"

struct BulletEx
{
    float r;
    float s;
    int a;
    int b;
    uint32_t type;
    int async;
};

struct BulletExState00_Speedup
{
    Timer timer;
    char unused1[16];
    float unused2;
    char unused3[12];
};
ASSERT_SIZE(BulletExState00_Speedup, 0x34);

struct BulletExState02_Accel
{
    Timer timer;
    float accelNorm;
    float accelAngle;
    D3DXVECTOR3 accelVector;
    int duration;
    char unused[8];
};
ASSERT_SIZE(BulletExState02_Accel, 0x34);

struct BulletExState03_AngleAccel
{
    Timer timer;
    float tangentialAccel;
    float angularVelocity;
    char unused[12];
    int maxTime;
    char unused2[8];
};
ASSERT_SIZE(BulletExState03_AngleAccel, 0x34);

struct BulletExState04_Turn
{
    Timer timer;
    float speed;
    float angleArgument;
    char unused[12];
    int timeInterval;
    int maxTurns;
    int turnCount;
};
ASSERT_SIZE(BulletExState04_Turn, 0x34);

struct BulletExState08_Bounce
{
    Timer timer;
    float speed;
    char unused[16];
    int bounceCount;
    int maxBounces;
    int wallMask;
};
ASSERT_SIZE(BulletExState08_Bounce, 0x34);

struct BulletExState12_Wait
{
    Timer timer;
    char unused[32];
};
ASSERT_SIZE(BulletExState12_Wait, 0x34);

struct BulletExState17
{
    Timer timer;
    char unused[32];
};
ASSERT_SIZE(BulletExState17, 0x34);

struct BulletExState18
{
    Timer timer;
    char unused[32];
};
ASSERT_SIZE(BulletExState18, 0x34);

struct BulletExState23
{
    Timer timer;
    float r;
    float s;
    uint32_t unk;
    int a_duration;
    uint32_t idk;
    uint32_t idk2;
    int unused[2];
};
ASSERT_SIZE(BulletExState23, 0x34);

struct BulletExState25_Move
{
    Timer timer;
    float finalSpeed;
    char unused[4];
    D3DXVECTOR3 finalPosition;
    int duration;
    int interpMode;
    char unused2[4];
};
ASSERT_SIZE(BulletExState25_Move, 0x34);

struct BulletExState27_VelAdditional
{
    Timer timer;
    uint32_t unused;
    D3DXVECTOR3 vec3;
    int duration;
    int idk[3];
};
ASSERT_SIZE(BulletExState27_VelAdditional, 0x34);

class Bullet
{
    uint32_t flags;
    uint32_t remainingInvulnerabilityFrames;
    AnmVm vm;
    D3DXVECTOR3 pos;
    D3DXVECTOR3 velovity;
    float speed;
    float angle;
    D3DXVECTOR2 hitbox;
    Timer timer0;
    Timer timer1;
    int ex_21a;
    uint32_t idk[4];
    uint32_t _4a0_init_10;
    uint32_t anmScript;
    uint32_t active_ex_flags;
    uint32_t initial_ex_flags;
    uint16_t idk1;
    uint16_t state;
    uint32_t idk2;
    Bullet* child;
    uint32_t unused;
    uint32_t idk_koishi_funcset_uses_it;
    uint32_t transform_sound;
    uint32_t ex_index;
    uint32_t field_c8_from_bullet_type_table;
    BulletEx ex[18];
    BulletExState00_Speedup exSpeedup;
    BulletExState02_Accel ExAccel;
    BulletExState03_AngleAccel exAngleAccel;
    BulletExState04_Turn exTurn;
    BulletExState08_Bounce exBounce;
    BulletExState12_Wait exWait;
    BulletExState17 ex17;
    BulletExState18 ex18;
    BulletExState23 ex23;
    BulletExState25_Move exMove;
    BulletExState27_VelAdditional exVelAdditional;
    uint32_t idk5;
    Interp<D3DXVECTOR3> exMove_i;
    short type;
    short color;
};
ASSERT_SIZE(Bullet, 0x910);