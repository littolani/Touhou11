#include "Supervisor.h"
#include "Globals.h"
#include "GameConfig.h"

int Supervisor::initializeDevices()
{
    return 0;
}

void Supervisor::releaseDinputIface()
{
    IDirectInput8A* iDirectInput8;
    iDirectInput8 = dInputInterface;
    if (iDirectInput8)
    {
        iDirectInput8->Release();
        dInputInterface = nullptr;
    }
}

void Supervisor::enterCriticalSection(size_t criticalSectionNumber)
{
    if (criticalSectionNumber >= 12)
        return;

    if ((criticalSectionFlag & 0x8000) != 0)
    {
        EnterCriticalSection(&criticalSections[criticalSectionNumber]);
        ++criticalSectionCounters[criticalSectionNumber];
    }
}

void Supervisor::leaveCriticalSection(size_t criticalSectionNumber)
{
    if (criticalSectionNumber >= 12)
        return;

    if ((criticalSectionFlag & 0x8000) != 0)
    {
        LeaveCriticalSection(&criticalSections[criticalSectionNumber]);
        --criticalSectionCounters[criticalSectionNumber];
    }
}

// 0x429eb0
int Supervisor::verifyGameConfig()
{
    size_t fileSize;
    byte* configFile = openFile("th11.cfg", &fileSize, 1);
    m_gameConfig = GameConfig();

    bool isValid = true;

    if (configFile == nullptr)
    {
        printf("Config file could not be found\n");
        isValid = false;
    }
    else
    {
        memcpy(&m_gameConfig, configFile, sizeof(GameConfig));
        delete[] configFile;

        if (m_gameConfig.colorDepth >= 2 ||
            m_gameConfig.sfxEnabled >= 3 ||
            m_gameConfig.startingBombs >= 2 ||
            m_gameConfig.displayMode >= 4 ||
            m_gameConfig.frameSkip >= 3 ||
            m_gameConfig.musicMode >= 3 ||
            m_gameConfig.version != 0x110003 ||
            fileSize != sizeof(GameConfig))
        {
            printf("Config file is invalid\n");
            isValid = false;
        }
    }

    if (!isValid)
        m_gameConfig = GameConfig(); // Reinitialize
    else
    {
        g_defaultGameConfig.shootKey = m_gameConfig.shootKey;
        g_defaultGameConfig.bombKey = m_gameConfig.bombKey;
        g_defaultGameConfig.focusKey = m_gameConfig.focusKey;
        g_defaultGameConfig.pauseKey = m_gameConfig.pauseKey;
        g_defaultGameConfig.upKey = m_gameConfig.upKey;
        g_defaultGameConfig.downKey = m_gameConfig.downKey;
        g_defaultGameConfig.leftKey = m_gameConfig.leftKey;
        g_defaultGameConfig.rightKey = m_gameConfig.rightKey;
        g_defaultGameConfig.refreshRate = m_gameConfig.refreshRate;
    }

    uint32_t flags = m_gameConfig.flags;

    if (flags & 0x1)
        printf("Using 16-bit textures.\n");

    if (m_d3dPresetParameters.Windowed != 0)
        printf("Starting in windowed mode.\n");

    if (flags & 0x2)
        printf("Force the reference rasterizer.\n");

    if (flags & 0x4)
        printf("Not using fog.\n");

    if (flags & 0x8)
        printf("DirectInput is not used for gamepad and keyboard input.\n");

    if (flags & 0x10)
        printf("Loading BGM into memory\n");

    m_noVerticalSyncFlag = 0;
    if (flags & 0x20) {
        printf("Vertical synchronization is not enabled.\n");
        m_noVerticalSyncFlag = 1;
    }

    if (flags & 0x40)
        printf("The text rendering environment is not automatically detected.\n");

    int writeStatus = writeToFile("th11.cfg", sizeof(GameConfig), &m_gameConfig);
    if (writeStatus != 0)
    {
        printf("Could not write file %s\n", "th11.cfg");
        printf("Is the disk full or write-protected?\n");
        return -1;
    }
    return 0;
}