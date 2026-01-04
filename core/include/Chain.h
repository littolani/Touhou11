#pragma once
#include <cstdint>
#include "Macros.h"

enum ChainCallbackResult
{
    CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB = 0, // Cut node and continue
    CHAIN_CALLBACK_RESULT_CONTINUE = 1,                // Normal step
    CHAIN_CALLBACK_RESULT_EXECUTE_AGAIN = 2,           // Immediate re-run (for logic that needs 2+ passes)
    CHAIN_CALLBACK_RESULT_BREAK = 3,                   // Stop processing this chain for this frame
    CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS = 4,       // Soft exit (e.g. Back to Menu)
    CHAIN_CALLBACK_RESULT_EXIT_GAME_ERROR = 5,         // Hard error exit (returns -1)
    CHAIN_CALLBACK_RESULT_RESTART_FROM_FIRST_JOB = 6,  // Reset 'tracker' to head
    CHAIN_CALLBACK_RESULT_CLEANUP_AND_CONTINUE = 7,    // Run cleanup callback at +0x10 and continue
    CHAIN_CALLBACK_RESULT_QUIT_GAME = 8                // Hard exit (Quit to OS)
};

// Fastcall calling convention must be used when calling functions in the game
// Once callbacks are hooked inside the DLL, however, they will be cdecl.
typedef ChainCallbackResult(__fastcall* ChainCallback)(void*);

struct ChainElem
{
    union {
        ChainElem* trackerJobNode;             // <0x0> Tracker: Points to job node
        int jobPriority;                       // <0x0> Job: Priority value
    };

    // Bit 0 of indicates whether the node is heap-allocated
    // Bit 1 indicates whether the node is a job node
    ChainElem* nextNode;                       // <0x4> Job/Tracker: Next node in list
    union {
        ChainCallback jobRunDrawChainCallback; // <0x8> Job: Callback
        ChainElem* trackerPrevNode;            // <0x8> Tracker: Previous node
    };
    ChainCallback registerChainCallback;       // <0xc> Job: Registration callback
    ChainCallback runCalcChainCallback;        // <0x10> Job: Calculation callback
    struct {
        ChainElem* trackerJobNode;             // <0x14> Tracker: Points to this job node
        ChainElem* trackerNextNode;            // <0x18> Tracker: Next tracker node
        ChainElem* trackerPrevNode;            // <0x1c> Tracker: Previous tracker node
    } embeddedTracker;
    void* args;                                // <0x20> Job: Callback arguments
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

    
    static int registerCalcChain(Chain* This, ChainElem* chainElem, int priority);

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

    static void removeTracker(ChainElem* tracker, ChainElem* elementToRemove);

};
ASSERT_SIZE(Chain, 0x4c);
