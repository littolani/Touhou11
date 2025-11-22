#include "GameConfig.h"

GameConfig g_defaultGameConfig;

GameConfig::GameConfig()
{
	// memset(this, 0,  sizeof(GameConfig));
	flags |= 0x100;
	padDeadzoneX  = 600;
	colorDepth    = 0;
	displayMode   = 0;
	frameSkip     = 0;
	version       = 0x110003;
	padDeadzoneY  = 600;
	sfxEnabled    = 1;
	startingBombs = 1;
	shootKey      = g_defaultGameConfig.shootKey;
	bombKey       = g_defaultGameConfig.bombKey;
	focusKey      = g_defaultGameConfig.focusKey;
	pauseKey      = g_defaultGameConfig.pauseKey;
	upKey         = g_defaultGameConfig.upKey;
	downKey       = g_defaultGameConfig.downKey;
	leftKey       = g_defaultGameConfig.leftKey;
	rightKey      = g_defaultGameConfig.rightKey;
	refreshRate   = g_defaultGameConfig.refreshRate;
	musicMode     = 2;
	latencyMode   = 2;
	isWindowed    = 0;
	bgmVolume     = 100;
	sfxVolume     = 80;
	windowPosX    = 0x80000000;
	windowPosY    = 0x80000000;
}
