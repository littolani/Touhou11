#pragma once
#include "Chireiden.h"
#include "Macros.h"
#include <cstdint>

enum ChainCallbackResult
{
    ContinueAndRemoveJob = 0, // Cut node and continue
    Continue = 1,             // Normal step
    ExecuteAgain = 2,         // Immediate re-run (for logic that needs 2+ passes)
    Break = 3,                // Stop processing this chain for this frame
    ExitGameSuccess = 4,      // Soft exit (e.g. Back to Menu)
    ExitGameError = 5,        // Hard error exit (returns -1)
    RestartFromFirstJob = 6,  // Reset 'tracker' to head
    CleanupAndContinue = 7,   // Run cleanup callback at +0x10 and continue
    QuitGame = 8              // Hard exit (Quit to OS)
};

// Fastcall calling convention must be used when calling functions in the game
// Once callbacks are hooked inside the DLL, however, they will be cdecl.
typedef ChainCallbackResult(__fastcall* ChainCallback)(void*);

template <typename T>
struct ZUNList
{
    T data;
    ZUNList<T>* nextNode;
    ZUNList<T>* prevNode;
};

struct ChainElem
{
    ChainElem()
    {
        //listNode.nextNode = (ZUNList<ChainElem*>*) ((uintptr_t)listNode.nextNode & ~1);
        listNode.data = this;
    }

    ChainElem(ChainCallback callback)
    {
        //listNode.nextNode = (ZUNList<ChainElem*>*) ((uintptr_t)listNode.nextNode & ~1);
        runCalcChainCallback = callback;
        listNode.data = this;
    }

    int32_t priority;           // 0x0
    uint32_t flags;
    //{
    //    uint32_t isHeapAllocated : 1;
    //    uint32_t runOnUpdate : 1;
    //};                              // 0x4
    ChainCallback runCalcChainCallback;           // 0x8
    ChainCallback registerChainCallback;           // 0xC
    ChainCallback cutCallback;           // 0x10
    ZUNList<ChainElem*> listNode;   // 0x14
    void* args;                      // 0x20
};

ASSERT_SIZE(ChainElem, 0x24);

struct TrackerNode
{
    ChainElem* ownerJob;   // <0x0> Points to the ChainElem (Base)
    TrackerNode* next;     // <0x4> Points to next embeddedTracker
    TrackerNode* prev;     // <0x8> Points to prev embeddedTracker
};

struct Chain
{
    ChainElem calcChain;      // <0x0> Sentinel node for calculation chain
    ChainElem drawChain;      // <0x24> Sentinel node for drawing chain
    uint32_t timeToRemove;    // <0x48> Time-to-remove flag

    Chain();
    static void release(Chain* This);

    /**
     * 0x456cb0
     * @param  This EBX:4
     * @return int  EAX:4
     */
    static int runCalcChain(Chain* This);

    /**
     * 0x456e10
     * @return int EAX:4
     */
    static int runDrawChain();

    /**
     * 0x456b70
     * @brief
     * @param  chainElem  ESI:4
     * @param  priority   EBX:4
     * @return int        EAX:4
     */
    static int registerCalcChain(ChainElem* chainElem, int priority);

    /**
     * 0x456c10
     * @brief
     * @param  chainElem  ESI:4
     * @param  priority   EBX:4
     * @return int        EAX:4
     */
    static int registerDrawChain(ChainElem* chainElem, int priority);

    static void releaseSingleChain(Chain* This, ChainElem* chainElem);

    static void cut(Chain* This, ChainElem* elementToRemove);

    static void removeTracker(ZUNList<ChainElem*>* tracker, ChainElem* elementToRemove);

};
ASSERT_SIZE(Chain, 0x4c);
