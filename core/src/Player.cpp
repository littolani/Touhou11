#include "Player.h"
#include "AnmManager.h"

int Player::shootOneBullet(Player* This, D3DXVECTOR3* position, int currentTime, Shooter* shooter)
{
    if (shooter->kind == 4 && This->shooterOptions[shooter->option] != 0)
        return 0;

    PlayerBullet* bullet = nullptr;
    int bulletIndex = 0;

    for (int i = 0; i < 256; ++i)
    {
        if (This->playerBullets[i].isActive == 0)
        {
            bullet = &This->playerBullets[i];
            bulletIndex = i + 1;
            break;
        }
    }

    if (!bullet)
        return 0; // No free bullets

    bullet->timer0.set(&bullet->timer0, 0);
    bullet->isActive = bulletIndex;
    bullet->shooter = shooter;
    bullet->damage = shooter->damage;

    if (shooter->option == 0) {
        bullet->position = *position;
    }
    else
    {
        auto& opt = This->playerOptions[shooter->option];
        bullet->position.x = opt.someOtherFloat.x * 0.0078125f;
        bullet->position.y = opt.someOtherFloat.y * 0.0078125f;
        bullet->position.z = 0.0f;
    }

    if (shooter->kind == 4)
        This->shooterOptions[shooter->option] = bulletIndex;

    bullet->speed = shooter->speed;

    // 0x43409a: Angle Calculation
    float finalAngle;
    if (shooter->angle < 1000.0f)
    {
        if (shooter->angle < 995.0f || shooter->option == 0)
            finalAngle = shooter->angle;
        else
            finalAngle = normalizeAngle(This->playerOptions[shooter->option].angle);
    }
    else {
        if (shooter->option == 0) {
            finalAngle = shooter->angle;
        }
        else {
            // 0x4340eb: Randomize angle
            uint32_t rngVal = AnmVm::rng(&g_replayRngContext);
            float rngFloat = (float)(int)rngVal;
            if ((int)rngVal < 0) rngFloat += 4294967296.0f;

            // 4.656613e-10 is approx 2^-31. rng * 2^-31 - 1.0 gives range [-1, 1].
            // Multiplied by PI/12 (15 degrees).
            float randomOffset = ((rngFloat * 4.6566129e-10f - 1.0f) * 3.1415927f) / 12.0f;
            finalAngle = randomOffset + This->playerOptions[shooter->option].angle;

            // Normalize
            finalAngle = normalizeAngle(finalAngle);
            bullet->angle = finalAngle; // Temporary storage?

            // 0x43412a: Randomize speed
            rngVal = AnmVm::rng(&g_replayRngContext);
            rngFloat = (float)(int)rngVal;
            if ((int)rngVal < 0) rngFloat += 4294967296.0f;

            float speedMod = rngFloat * 4.6566129e-10f - 1.0f;
            bullet->speed = speedMod + speedMod + shooter->speed; // speed + 2 * random?
        }
    }

    // Normalize and store final angle
    bullet->angle = normalizeAngle(finalAngle);

    // 0x43415a: Velocity Calculation
    if ((bullet->flags & 1) == 0)
    {
        decomposeSpeedMagnitudeIntoVelocityComponents(&bullet->velocity, bullet->angle, bullet->speed);
        bullet->velocity.z = 0.0f;
    }
    else {
        // Custom movement logic (smoothing)
        bullet->smth += bullet->d_smth_dt;
        float combined = bullet->speed + bullet->angle;
        bullet->angle = normalizeAngle(combined);
        // Note: Assembly path for flags & 1 seems to skip standard velocity decomposition
    }

    // 0x4341da: Apply Offset
    // Position = Position + Offset - Velocity
    bullet->position.x += shooter->offset.x - bullet->velocity.x;
    bullet->position.y += shooter->offset.y - bullet->velocity.y;

    // 0x4341fb: Create ANM Script
    // Script index is shooter->anmScript + 5
    int scriptIndex = shooter->anmScript + 5;
    AnmId vmId;

    AnmManager::makeVmWithAnmLoaded(This->playerAnm, scriptIndex, 0x16, &vmId);

    bullet->anmId4c = vmId;

    AnmVm* vm = nullptr;
    if (vmId.id != 0)
    {
        for (AnmVmList* node = g_anmManager->m_primaryGlobalNext; node; node = node->next)
        {
            if (node->entry->m_id.id == vmId.id) {
                vm = node->entry;
                goto VmFound;
            }
        }

        for (AnmVmList* node = g_anmManager->m_secondaryGlobalNext; node; node = node->next)
        {
            if (node->entry->m_id.id == vmId.id) {
                vm = node->entry;
                goto VmFound;
            }
        }
    }

    // If not found, clear IDs
    bullet->anmId4c.id = 0;
    goto PostVmLogic;

VmFound:
    if (vm->m_flagsLow & 0x8000000)
    {
        vm->m_rotation.z = shooter->angle;
        vm->m_flagsLow |= 4;
    }
    bullet->anmId50.id = 0;

    vm->m_entityPos.x = bullet->position.x + 224.0f;
    vm->m_entityPos.y = bullet->position.y + 16.0f;
    vm->m_entityPos.z = bullet->position.z;

PostVmLogic:
    // 0x434280: Run OnInit callback
    if (shooter->onInit) {
        typedef void (*ShooterCallback)(int);
        ((ShooterCallback)shooter->onInit)(currentTime);
    }

    if (shooter->sfx >= 0)
        SoundManager::playSoundWithPan(bullet->position.x, shooter->sfx);

    return 0;
}

#if 0
int Player::move(Player* This)
{
    // Bits: 0x10=Up, 0x20=Down, 0x40=Left, 0x80=Right
    uint8_t inputFlags = _g_playerShootingRelatedFlag;
    int attemptedDirection = 0;

    // Check diagonals first (specific bit combinations)
    if ((inputFlags & 0x50) == 0x50) attemptedDirection = 5; // Up-Left
    else if ((inputFlags & 0x60) == 0x60) attemptedDirection = 7; // Up-Right
    else if ((inputFlags & 0x90) == 0x90) attemptedDirection = 6; // Down-Left
    else if ((inputFlags & 0xA0) == 0xA0) attemptedDirection = 8; // Down-Right
    else
    {
        bool isDown = (inputFlags & 0x20);
        bool isUp = (inputFlags & 0x10);
        bool isLeft = (inputFlags & 0x40);
        bool isRight = (inputFlags & 0x80);

        if (!isDown)
        {
            if (!isUp)
            {
                if (!isLeft)
                {
                    if (isRight)
                        attemptedDirection = 4; // Right
                    else
                        attemptedDirection = 0; // Neutral
                }
                else
                    attemptedDirection = 3; // Left
            }
            else
                attemptedDirection = 1; // Up
        }
        else
            attemptedDirection = 2; // Down
    }

    This->attempted_direction = attemptedDirection;


    bool enemiesActive = (g_enemyManager && g_enemyManager->someIndicator != 0);
    bool canFocus = enemiesActive || (This->timer2.current >= 4);

    if (!canFocus) {
        This->isFocused = 0;
        This->percentMovedByOptions = 30; // 0x1e: Force a specific state/timer
    }
    else {
        // Bit 3 (0x08) usually corresponds to the Focus key (Shift)
        This->isFocused = (inputFlags >> 3) & 1;
    }

    int velX = 0;
    int velY = 0;

    // Handle Reimu A's teleporting at edges
    if (This->reimu_a_gapping_state == 99) { // 0x63
        velX = -150;
    }
    else if (This->reimu_a_gapping_state == 100) { // 0x64
        velX = 150;
    }
    else {
        // Standard Movement
        // Manage Hitbox visibility based on focus state
        if (This->isFocused == 0) {
            if (This->anmIdFocusedHitbox.id != 0) {
                // Retrieve VM and kill/hide it
                AnmVm* vm = AnmManager::getVmWithId(g_anmManager, This->anmIdFocusedHitbox.id);
                if (vm) AnmManager::setInterruptById(This->anmIdFocusedHitbox.id, 1);
                This->anmIdFocusedHitbox.id = 0;
            }
        }
        else {
            if (This->anmIdFocusedHitbox.id == 0) {
                // Create Hitbox VM (Script 11 in Bullet ANM)
                AnmId newId;
                AnmManager::makeVmWithAnmLoaded(g_bulletManager->bulletAnm, 74, 11, &newId);
                This->anmIdFocusedHitbox.id = newId.id;
            }
        }

        // Determine Speed
        int speed = This->isFocused ? This->focusSpeedSubpixel : This->normalSpeedSubpixel;
        int diagSpeed = This->isFocused ? This->focusSpeedSubpixelOverSqrt2 : This->normalSpeedSubpixelOverSqrt2;

        // Apply Velocity
        switch (attemptedDirection) {
        case 1: velY = -speed; break;     // Up
        case 2: velY = speed; break;     // Down
        case 3: velX = -speed; break;     // Left
        case 4: velX = speed; break;     // Right
        case 5: velX = -diagSpeed; velY = -diagSpeed; break; // UL
        case 6: velX = diagSpeed; velY = -diagSpeed; break; // UR
        case 7: velX = -diagSpeed; velY = diagSpeed; break; // DL
        case 8: velX = diagSpeed; velY = diagSpeed; break; // DR
        }
    }

    // Sprite Banking Animation
    int prevVelX = This->attempted_velocity__internal.i0;
    int scriptToLoad = -1;
    bool isUnfocusedMode = (This->someFlag & 2) == 0;

    if (isUnfocusedMode) {
        // Unfocused Scripts: 0=Neutral, 1=ToLeft, 2=FromLeft, 3=ToRight, 4=FromRight
        if (velX < 0 && prevVelX >= 0) {
            scriptToLoad = 1; // Start turning Left
        }
        else if (velX > 0 && prevVelX <= 0) {
            scriptToLoad = 3; // Start turning Right
        }
        else if (velX == 0) {
            if (prevVelX < 0) scriptToLoad = 2;      // Return to Neutral from Left
            else if (prevVelX > 0) scriptToLoad = 4; // Return to Neutral from Right
        }
    }
    else {
        // Focused Scripts: 27=ToLeft, 28=FromLeft, 29=ToRight, 30=FromRight
        if (velX < 0 && prevVelX >= 0) {
            scriptToLoad = 27;
        }
        else if (velX > 0 && prevVelX <= 0) {
            scriptToLoad = 29;
        }
        else if (velX == 0) {
            if (prevVelX < 0) scriptToLoad = 28;
            else if (prevVelX > 0) scriptToLoad = 30;
        }
    }

    // If a transition is needed, load it into the main player VM (vm0)
    if (scriptToLoad != -1) {
        AnmVm::loadIntoAnmVm(&This->vm0, This->playerAnmLoaded, scriptToLoad);
    }

    // Store velocities for next frame logic
    This->attempted_velocity__internal.i0 = velX;
    This->attempted_velocity__internal.i1 = velY;


    // Reimu C: Speed boost if no shooting/focus for 10 frames
    if (g_globals.subshot == 2 && g_globals.character == 0) {
        if ((inputFlags & 9) == 0) { // Bit 0 (Shoot/Z) and Bit 3 (Focus/Shift) are 0
            This->reimuC_FramesWithout_Z_Or_Shift++;
            if (This->reimuC_FramesWithout_Z_Or_Shift > 10) {
                velX *= 2;
                velY *= 2;
                // Create speed effect
                AnmId effectId;
                AnmManager::makeVmWithAnmLoaded(This->playerAnmLoaded, 19, 11, &effectId);
                AnmVm* effectVm = AnmManager::getVmWithId(g_anmManager, effectId.id);
                if (effectVm && effectVm->anmLoaded) {
                    AnmVm::setupTextureQuadAndMatrices(effectVm, This->vm0.spriteNumber, effectVm->anmLoaded);
                }
                transformVm(&This->position, effectVm);
            }
        }
        else {
            This->reimuC_FramesWithout_Z_Or_Shift = 0;
        }
    }

    // Calculate Delta Position (Float)
    This->attemptedDeltaPosSubpixel.x = (float)velX * g_gameSpeed;
    This->attemptedDeltaPosSubpixel.y = (float)velY * g_gameSpeed;

    if (attemptedDirection != 0) {
        This->lastNonzeroAttemptedDeltaPosSubpixel = This->attemptedDeltaPosSubpixel;
    }

    // Apply Position (Subpixel integer math)
    int moveX = roundFloat(This->attemptedDeltaPosSubpixel.x);
    int moveY = roundFloat(This->attemptedDeltaPosSubpixel.y);

    This->posSubpixel.i0 += moveX;
    This->posSubpixel.i1 += moveY;

    // Reimu A: Gap Boundary Logic (Wrap around screen edges)
    if (This->reimu_a_gapping_state < 99) {
        int xLimit = 23552; // 0x5c00 (Approx 184.0f * 128)

        if (This->posSubpixel.i0 < -xLimit) {
            if (This->reimu_a_gapping_state == 0 && g_globals.subshot == 0 && g_globals.character == 0) {
                This->reimu_a_gapping_state = 1; // Trigger Teleport Left->Right
                This->reimu_a_frames_in_gapping_state = 0;
            }
            This->posSubpixel.i0 = -xLimit;
        }
        else if (This->posSubpixel.i0 > xLimit) {
            if (This->reimu_a_gapping_state == 0 && g_globals.subshot == 0 && g_globals.character == 0) {
                This->reimu_a_gapping_state = 3; // Trigger Teleport Right->Left
                This->reimu_a_frames_in_gapping_state = 0;
            }
            This->posSubpixel.i0 = xLimit;
        }

        // Clamp Y Position
        if (This->posSubpixel.i1 < 0x1000) This->posSubpixel.i1 = 0x1000;
        else if (This->posSubpixel.i1 > 0xd800) This->posSubpixel.i1 = 0xd800;
    }

    // Convert Subpixels to Float for Rendering (1/128 = 0.0078125)
    This->position.x = (float)This->posSubpixel.i0 * 0.0078125f;
    This->position.y = (float)This->posSubpixel.i1 * 0.0078125f;

    // Update Hitbox Position
    AnmVm* hitboxVm = AnmManager::getVmWithId(g_anmManager, This->anmIdFocusedHitbox.id);
    if (!hitboxVm) {
        This->anmIdFocusedHitbox.id = 0;
    }
    else {
        D3DXVECTOR3 hitboxVec;
        // The game often adds offsets (like +32+192) to move from game-space to screen-space 
        hitboxVec.x = This->position.x + 224.0f;
        hitboxVec.y = This->position.y + 16.0f;
        hitboxVec.z = This->position.z;

        // Update the VM's position
        AnmManager::setEntityPositionById(g_anmManager, (int)g_anmManager, &hitboxVec);
    }

    // TODO: Handle Options and Marisa B specific formation toggles
    return 0;
}
#endif

void Player::setIframes(int currentTime)
{
    g_player->timerIFrames.set(&g_player->timerIFrames, currentTime);
}
