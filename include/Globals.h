#pragma once
#include "Chireiden.h"
#include "Chain.h"
#include "FileAbstrction.h"
#include "Supervisor.h"
#include "AnmManager.h"

extern HANDLE g_app;
extern HWND g_window;
extern DWORD g_unusualLaunchFlag;
extern HINSTANCE g_hInstance;
extern DWORD g_primaryScreenWorkingArea;
extern DWORD g_mouseSpeed;
extern DWORD g_screenWorkingArea;
extern LARGE_INTEGER g_performanceFrequency;
extern LARGE_INTEGER g_performanceCount;
extern PbgArchive g_pbgArchive; 
extern float g_gameSpeed;
extern double g_time;
extern int g_numEntriesInDatFile;
extern PbgArchive g_pbgArchives[20];

double getDeltaTime();
int fileExists(LPCSTR filePath);
int createDirectory(LPCSTR pathName);
int writeToFile(LPCSTR fileName, DWORD numBytes, LPVOID bytes);
byte* openFile(const char* filename, size_t* outSize, BOOL isExternalResource);
PbgArchive* findMatchingArchive(const char* filename);