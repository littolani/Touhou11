#include "Supervisor.h"

Supervisor g_supervisor;

int Supervisor::initializeDevices()
{
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

int Supervisor::verifyGameConfig()
{
    byte* th11ConfigFile;
    int status;
    int loopCounter;
    uint32_t* th11ConfigFileCopy;
    uint32_t* start;
    size_t outSize;
    const char* errString;

    GameConfig::GameConfig(&gameConfig);
    /* ???ok so we took the supervisor and put it in supervisorCopy and am now using
       this param as the outSize... */
    th11ConfigFile = openFile("th11.cfg", &outSize, 1);
    if (th11ConfigFile == (byte*)0x0) {
        errString = "コンフィグデータが見つからないので初期化しました\r\n";
    }
    else {
        loopCounter = 0xf;
        th11ConfigFileCopy = (uint32_t*)th11ConfigFile;
        start = &g_supervisor.gameConfig.version;
        /* movsd.rep copy */
        for (; loopCounter != 0; loopCounter = loopCounter + -1) {
            *start = *th11ConfigFileCopy;
            th11ConfigFileCopy = th11ConfigFileCopy + 1;
            start = start + 1;
        }
        _free(th11ConfigFile);
        if (((((g_supervisor.gameConfig.idk6 < 2) && (g_supervisor.gameConfig.volumeMaybe < 3)) &&
            (g_supervisor.gameConfig.lifeCount < 2)) &&
            ((g_supervisor.gameConfig.windowSettings < 4 && (g_supervisor.gameConfig.frameSkip < 3)))) &&
            ((g_supervisor.gameConfig.musicMode < 3 &&
                ((g_supervisor.gameConfig.version == 0x110003 && (supervisorThenOutSize == (Supervisor*)0x3c))
                    )))) {
            /* restore original config */
            g_defaultGameConfig.idk9 = g_supervisor.gameConfig.idk9;
            g_defaultGameConfig.idk12 = g_supervisor.gameConfig.idk12;
            g_defaultGameConfig.idk13 = g_supervisor.gameConfig.idk13;
            g_defaultGameConfig.idk14 = g_supervisor.gameConfig.idk14;
            g_defaultGameConfig.idk8 = g_supervisor.gameConfig.idk8;
            g_defaultGameConfig.idk10 = g_supervisor.gameConfig.idk10;
            g_defaultGameConfig.idk7 = g_supervisor.gameConfig.idk7;
            g_defaultGameConfig.idk11 = g_supervisor.gameConfig.idk11;
            g_defaultGameConfig.refreshRate = g_supervisor.gameConfig.refreshRate;
            goto LoggingPortion;
        }
        errString = "コンフィグデータが異常でしたので再初期化しました\r\n";
    }
    FN_logToGlobalBuffer(&g_loggingBuffer, errString, vaList);
    GameConfig::GameConfig(&g_supervisor.gameConfig);

LoggingPortion:
    supervisorCopy->noVerticalSyncFlag = 0;
    if (((supervisorCopy->gameConfig).optionsFlag & 4) != 0)
        printf("フォグの使用を抑制します\n");

    if (((supervisorCopy->gameConfig).optionsFlag & 1) != 0)
        printf("16Bit のテクスチャの使用を強制します\n");

    if ((supervisorCopy->d3dPresetParameters).Windowed != 0)
        printf("ウィンドウモードで起動します\n");

    if (((supervisorCopy->gameConfig).optionsFlag & 2) != 0)
        printf("リファレンスラスタライザを強制します\n", vaList);

    if (((supervisorCopy->gameConfig).optionsFlag & 8) != 0)
        printf("パッド、キーボードの入力に DirectInput を使用しません\n");

    if (((supervisorCopy->gameConfig).optionsFlag & 0x10) != 0)
        printf("ＢＧＭをメモリに読み込みます\n");

    if (((supervisorCopy->gameConfig).optionsFlag & 0x20) != 0)
    {
        printf("垂直同期を取りません\r\n");
        g_supervisor.noVerticalSyncFlag = 1;
    }

    if (((supervisorCopy->gameConfig).optionsFlag & 0x40) != 0)
        printf("文字描画の環境を自動検出しません\n");

    status = writeToFile("th11.cfg", 0x3c, &g_supervisor.gameConfig);
    if (status != 0)
    {
        printf("ファイルが書き出せません %s\n");
        printf("フォルダが書込み禁止属性になっているか、ディスクがいっぱいいっぱいになってませんか？\n");
        return -1;
    }
    return 0;
}

