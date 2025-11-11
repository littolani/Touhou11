#include "GameConfig.h"

GameConfig g_defaultGameConfig;

GameConfig::GameConfig()
{
	uint32_t temp;
	memset(this, 0, 0x3c);
	temp._0_1_ = config->optionsFlag;
	temp._1_1_ = config->idk3;
	temp._2_1_ = config->idk4;
	temp._3_1_ = config->idk5;
	temp = temp | 0x100;
	config->optionsFlag = (char)temp;
	config->idk3 = (char)(temp >> 8);
	config->idk4 = (char)(temp >> 0x10);
	config->idk5 = (char)(temp >> 0x18);
	config->screenWidth = 600;
	config->idk6 = '\0';
	config->windowSettings = '\0';
	config->frameSkip = '\0';
	config->version = 0x110003;
	config->screenHeight = 600;
	config->volumeMaybe = '\x01';
	config->lifeCount = '\x01';
	config->idk9 = g_defaultGameConfig.idk9;
	config->idk12 = g_defaultGameConfig.idk12;
	config->idk13 = g_defaultGameConfig.idk13;
	config->idk14 = g_defaultGameConfig.idk14;
	config->idk8 = g_defaultGameConfig.idk8;
	config->idk10 = g_defaultGameConfig.idk10;
	config->idk7 = g_defaultGameConfig.idk7;
	config->idk11 = g_defaultGameConfig.idk11;
	config->refreshRate = g_defaultGameConfig.refreshRate;
	config->musicMode = 2;
	config->frameSkipConfig = 2;
	config->windowed = 0;
	config->bgmVolume = 100;
	config->sfxVolume = 80;
	config->screenPosX = 0x80000000;
	config->screenPosY = 0x80000000;
}