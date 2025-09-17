#pragma once

#include "Chireiden.h"
#include "Globals.h"

enum ChainCallbackResult
{
    CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB = 0,
    CHAIN_CALLBACK_RESULT_CONTINUE,
    CHAIN_CALLBACK_RESULT_EXECUTE_AGAIN,
    CHAIN_CALLBACK_RESULT_BREAK,
    CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS,
    CHAIN_CALLBACK_RESULT_EXIT_GAME_ERROR,
    CHAIN_CALLBACK_RESULT_RESTART_FROM_FIRST_JOB,
    UNKNOWN_8
};

typedef ChainCallbackResult(*ChainCallback)(void*);

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

struct Chain
{
    ChainElem calcChain;      // <0x0> Sentinel node for calculation chain
    ChainElem drawChain;      // <0x24> Sentinel node for drawing chain
    uint32_t timeToRemove;    // <0x48> Time-to-remove flag

    Chain() = default;
    void release();
    int runCalcChain();
    int runDrawChain();
    int registerCalcChain(ChainElem* chainElem, int priority);
    int registerDrawChain(ChainElem* chainElem, int priority);
    void releaseSingleChain(ChainElem* chainElem);
    void cut(ChainElem* elementToRemove);
    static void removeTracker(ChainElem* tracker, ChainElem* elementToRemove);

};
ASSERT_SIZE(Chain, 0x4c);

extern Chain* g_chain;
