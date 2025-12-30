#include "Globals.h"
#include "AnmManager.h"
#include "Supervisor.h"
#include "AsciiManager.h"
#include "Chain.h"
#include "FileAbstraction.h"

HANDLE g_app;
DWORD g_unusualLaunchFlag{};
HINSTANCE g_hInstance{};
DWORD g_primaryScreenWorkingArea{};
DWORD g_mouseSpeed{};
DWORD g_screenWorkingArea{};
LARGE_INTEGER g_performanceFrequency{};
LARGE_INTEGER g_performanceCount{};
D3DFORMAT g_d3dFormats[] = { D3DFMT_UNKNOWN, D3DFMT_A8R8G8B8, D3DFMT_A1R5G5B5, D3DFMT_R5G6B5, D3DFMT_R8G8B8, D3DFMT_A4R4G4B4 };
uint32_t g_bytesPerPixelLookupTable[] = { 4, 4, 2, 2, 3, 2, 0, 1, 2 };
double g_time = 0.0;
float g_gameSpeed = 1.0;
RngContext g_anmRngContext;
RngContext g_replayRngContext;

double getDeltaTime()
{
    g_supervisor.enterCriticalSection(5);
    double elapsedTime;
    if (g_performanceFrequency.QuadPart != 0)
    {
        LARGE_INTEGER currentCounter;
        QueryPerformanceCounter(&currentCounter);
        int64_t elapsedTicks = currentCounter.QuadPart - g_performanceCount.QuadPart;
        elapsedTime = static_cast<double>(elapsedTicks) / g_performanceFrequency.QuadPart;

        if (elapsedTime < g_time) {
            g_time = elapsedTime;
        }
        elapsedTime -= g_time;
    } 
    else
    {
        double currentTime = timeGetTime();
        if (currentTime < 0)
            currentTime += 4294967296.0;
        if (g_time > currentTime)
            g_time = currentTime;
        elapsedTime = (currentTime - g_time) / 1000.0;
    }
    g_supervisor.leaveCriticalSection(5);
    return elapsedTime;
}

byte* openFile(const char* filename, size_t* outSize, BOOL isExternalResource)
{
    printf("Opening %s\n", filename);
    byte* outBuffer;
    int decompressedSize;
    
    g_supervisor.enterCriticalSection(2);

    // Find existing file
    if (isExternalResource == 0)
    {
        // Finds last backslash in path
        const char* lastBackslash = strrchr(filename, '\\');
        const char* baseFilename = filename;
        if (lastBackslash)
            baseFilename = lastBackslash + 1;
        
        // Finds last forwardslash in path
        baseFilename = strrchr(baseFilename, '/');
        if (baseFilename)
            filename = baseFilename + 1;

        PbgArchiveEntry* entries = g_pbgArchive.m_entries;
        if (!entries)
        {
            g_supervisor.leaveCriticalSection(2);
            return nullptr;
        }

        bool foundArchive = false;
        int i = g_pbgArchive.m_numEntries;
        while (i)
        {
            if (_stricmp(filename, entries->filename) == 0)
            {
                foundArchive = true;
                break;
            }
            --i;
            ++entries;
        }

        if (!foundArchive)
        {
            printf("openFile: Archive not found!\n");
            if (outSize)
                *outSize = 0;
            return nullptr;
        }

        decompressedSize = entries->decompressedSize;
        if (!decompressedSize)
        {
            g_supervisor.leaveCriticalSection(2);
            return nullptr;
        }

        // Update outSize if it's asked for
        if (outSize) 
            *outSize = decompressedSize;

        // Decrypt entry
        printf("Decoding %s\n", filename);
        //outBuffer = new byte[decompressedSize];
        outBuffer = (byte*)game_malloc(decompressedSize);

        if (!outBuffer)
        {
            printf("openFile: Buffer allocation failed!\n");
            g_supervisor.leaveCriticalSection(2);
            return nullptr;
        }
        g_pbgArchive.readDecompressEntry(&g_pbgArchive, outBuffer, filename);
        g_supervisor.leaveCriticalSection(2);
        return outBuffer;
    }

    // Load from file
    else
    {
        printf("Loading %s\n", filename);
        HANDLE file = CreateFileA(
            filename,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (file == INVALID_HANDLE_VALUE)
        {
            printf("Error: %s was not found.\n", filename);
            g_supervisor.leaveCriticalSection(2);
            return nullptr;
        }

        DWORD fileSize = GetFileSize(file,NULL);

        //outBuffer = new byte[fileSize];
        outBuffer = (byte*)game_malloc(fileSize);

        if (!outBuffer)
        {
            printf("Error: %s allocation error.\n", filename);
            CloseHandle(file);
            g_supervisor.leaveCriticalSection(2);
            return nullptr;
        }

        ReadFile(
            file,
            outBuffer,
            fileSize,
            &fileSize,
            NULL
        );

        if (outSize)
            *outSize = fileSize;

        CloseHandle(file);
        g_supervisor.leaveCriticalSection(2);
        return outBuffer;

    }
}

// 0x458670
int writeToFile(LPCSTR fileName, DWORD numBytes, LPVOID bytes)
{
    DWORD dwMessageId;
    DWORD dwLanguageId;
    DWORD nSize;
    DWORD numBytesWritten;
    
    g_supervisor.enterCriticalSection(2);
    HANDLE hFile = CreateFileA(
        fileName,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE)
    {
        nSize = 0;
        dwLanguageId = 0x400;
        dwMessageId = GetLastError();
        FormatMessageA(
            0x1300,
            NULL,
            dwMessageId,
            dwLanguageId,
            (LPSTR)&bytes,
            nSize,
            NULL
        );
        printf("error : %s write error\n",fileName);
        LocalFree(bytes);
        g_supervisor.leaveCriticalSection(2);
        return -1;
    }

    WriteFile(hFile,
        bytes,
        numBytes,
        &numBytesWritten,
        NULL
    );

    if (numBytes != numBytesWritten)
    {
        CloseHandle(hFile);
        printf("error : %s write error\n",fileName);
        g_supervisor.leaveCriticalSection(2);
        return -2;
    }
    CloseHandle(hFile);
    printf("%s write ...\n",fileName);
    g_supervisor.leaveCriticalSection(2);
    return 0;
}

// 0x460885
int createDirectory(LPCSTR pathName)
{
    BOOL status = CreateDirectoryA(pathName, NULL);
    if (!status)
    {
        uint32_t error = GetLastError();
        std::cerr << error << "\n";
        return -1;
    }
    return 0;
}

// 0x4585e0
int fileExists(LPCSTR filePath)
{
    g_supervisor.enterCriticalSection(2);
    HANDLE fileHandle = CreateFileA(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
        g_supervisor.leaveCriticalSection(2);
        return 1;
    }

    g_supervisor.leaveCriticalSection(2);
    return 0;
}

void loadTh11Dat()
{
    size_t outSize;
    char buffer[128];

    if (!g_pbgArchive.load("th11.dat"))
    {
        printf("Could not save data file\n");
        return;
    }
    sprintf_s(buffer, "th11_%.4x%c.ver", 0x100, 0x61);
    g_supervisor.th11DatBytes = openFile(buffer, &outSize, 0);
    g_supervisor.th11DatSize = outSize;
    if (!g_supervisor.th11DatBytes)
    {
        printf("Data version is incorrect\n");
        g_supervisor.th11DatBytes = nullptr;
    }
}
