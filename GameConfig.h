#pragma once
#include "Chireiden.h"
#include "Macros.h"

struct GameConfig
{
    uint32_t version;
    uint16_t idk9;
    uint16_t idk12;
    uint16_t idk13;
    uint16_t idk14;
    uint16_t idk8;
    uint16_t idk10;
    uint16_t idk7;
    uint16_t idk11;
    uint16_t refreshRate;	    // Confirmed
    uint16_t screenWidth;	    // Confirmed
    uint16_t screenHeight;	    // Confirmed
    uint8_t idk6;
    uint8_t volumeMaybe;
    uint8_t lifeCount;
    uint8_t windowSettings;
    uint8_t frameSkip;	        // Confirmed
    uint8_t musicMode;	        // Confirmed
    uint8_t bgmVolume;
    uint8_t sfxVolume;	        // Confirmed
    uint8_t windowed;	        // Confirmed
    uint8_t frameSkipConfig;	// Confirmed
    uint16_t  padXaxis;
    uint16_t  paxYAxis;
    uint32_t  screenPosX;	    // Confirmed
    uint32_t  screenPosY;	    // Confirmed
    uint32_t  idk1;
    uint32_t  idk2;
    byte optionsFlag;
    uint8_t idk3;
    uint8_t idk4;
    uint8_t idk5;

    GameConfig();
};
ASSERT_SIZE(GameConfig, 0x3c);

extern GameConfig g_defaultGameConfig;