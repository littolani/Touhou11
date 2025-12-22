#include "Chain.h"
#include "Globals.h"
#include "Supervisor.h"

Chain::Chain()
{
    calcChain.trackerPrevNode = nullptr;
    calcChain.registerChainCallback = nullptr;
    calcChain.jobPriority = 0;
    calcChain.nextNode = (ChainElem*)((uintptr_t)&calcChain & ~1);
    calcChain.embeddedTracker.trackerNextNode = nullptr;
    calcChain.embeddedTracker.trackerPrevNode = nullptr;
    calcChain.embeddedTracker.trackerJobNode = &calcChain;
    drawChain.trackerPrevNode = nullptr;
    drawChain.registerChainCallback = nullptr;
    drawChain.jobPriority = 0;
    drawChain.nextNode = (ChainElem*)((uintptr_t)&drawChain & ~1);
    drawChain.embeddedTracker.trackerNextNode = nullptr;
    drawChain.embeddedTracker.trackerPrevNode = nullptr;
    drawChain.embeddedTracker.trackerJobNode = &drawChain;
    timeToRemove = 0;
}

void Chain::release()
{
    //Supervisor::closeThread(&g_supervisor.loadingThread);
    timeToRemove = 1;
    runCalcChain();
    releaseSingleChain(&calcChain);
    releaseSingleChain(&drawChain);
    drawChain.jobRunDrawChainCallback = nullptr;
    drawChain.registerChainCallback = nullptr;
    drawChain.runCalcChainCallback = nullptr;
    calcChain.jobRunDrawChainCallback = nullptr;
    calcChain.registerChainCallback = nullptr;
    calcChain.runCalcChainCallback = nullptr;
}

void Chain::releaseSingleChain(ChainElem* root)
{
    ChainElem* elem = (root->embeddedTracker).trackerNextNode;
    while (elem != nullptr)
    {
        ChainElem* branch = elem->trackerJobNode;
        branch = branch->nextNode;
        if (branch != nullptr)
        {
            g_supervisor.enterCriticalSection(0);
            cut(branch);
            g_supervisor.leaveCriticalSection(0);
         }
    }
}

void Chain::cut(ChainElem* elementToRemove)
{
    if (elementToRemove == nullptr)
        return;

    // Search calcChain for the tracker node pointing to elementToRemove
    ChainElem* tracker = reinterpret_cast<ChainElem*>(&this->calcChain.embeddedTracker);
    while (tracker != nullptr)
    {
        if (tracker->trackerJobNode == elementToRemove)
        {
            removeTracker(tracker, elementToRemove);
            return;
        }
        tracker = tracker->nextNode;
    }

    // If not found, search drawChain
    tracker = reinterpret_cast<ChainElem*>(&this->drawChain.embeddedTracker);
    while (tracker != nullptr)
    {
        if (tracker->trackerJobNode == elementToRemove)
        {
            removeTracker(tracker, elementToRemove);
            return;
        }
        tracker = tracker->nextNode;
    }
}

void Chain::removeTracker(ChainElem* tracker, ChainElem* elementToRemove)
{
    // Update doubly-linked list pointers to remove tracker node
    if (tracker->trackerPrevNode != nullptr)
    {
        if (tracker->nextNode != nullptr)
            tracker->nextNode->trackerPrevNode = tracker->trackerPrevNode;

        if (tracker->trackerPrevNode != nullptr)
            tracker->trackerPrevNode->nextNode = tracker->nextNode;

        tracker->nextNode = nullptr;
        tracker->trackerPrevNode = nullptr;
    }

    // Cleanup the job node (elementToRemove)
    elementToRemove->jobRunDrawChainCallback = nullptr;
    if ((reinterpret_cast<uintptr_t>(elementToRemove->nextNode) & 1) != 0)
    {
        // Heap-allocated job node: clear additional fields and free
        elementToRemove->jobRunDrawChainCallback = nullptr;
        elementToRemove->registerChainCallback = nullptr;
        elementToRemove->runCalcChainCallback = nullptr;
        free(elementToRemove);
    }
}

int Chain::registerCalcChain(ChainElem* chainElem, int priority)
{
    int registerCallbackResult = 0;
    if (chainElem->registerChainCallback != nullptr) {
        registerCallbackResult = (*chainElem->registerChainCallback)(chainElem->args);
        chainElem->registerChainCallback = nullptr;
    }
    g_supervisor.enterCriticalSection(0);

    chainElem->jobPriority = priority;
    //ChainElem* tracker = &g_chain->calcChain;  // Sentinel node
    ChainElem* tracker = &this->calcChain;  // Sentinel node
    ChainElem* trackerNext = tracker->nextNode;

    while (trackerNext)
    {
        ChainElem* jobNode = trackerNext->trackerJobNode;
        if (jobNode->jobPriority >= priority)
            break;
        tracker = trackerNext;
        trackerNext = trackerNext->nextNode;
    }

    // Use the embedded tracker node within chainElem
    ChainElem* newTracker = reinterpret_cast<ChainElem*>(&chainElem->embeddedTracker);
    newTracker->nextNode = tracker->nextNode; //TODO: Verify newTracker->trackerJobNode = chainElem is set elsewhere?
    if (tracker->nextNode != nullptr)
        tracker->nextNode->trackerPrevNode = newTracker;

    tracker->nextNode = newTracker;
    newTracker->trackerPrevNode = tracker;

    g_supervisor.leaveCriticalSection(0);
    return registerCallbackResult;
}

int Chain::registerDrawChain(ChainElem* chainElem, int priority)
{
    int registerCallbackResult = 0;
    if (chainElem->registerChainCallback != nullptr) {
        registerCallbackResult = (*chainElem->registerChainCallback)(chainElem->args);
        chainElem->registerChainCallback = nullptr;
    }
    g_supervisor.enterCriticalSection(0);

    chainElem->jobPriority = priority;
    // ChainElem* tracker = &g_chain->drawChain;  // Sentinel node
    ChainElem* tracker = &this->drawChain;
    ChainElem* trackerNext = tracker->nextNode;

    while (trackerNext)
    {
        ChainElem* jobNode = trackerNext->trackerJobNode;
        if (jobNode->jobPriority >= priority)
            break;
        tracker = trackerNext;
        trackerNext = trackerNext->nextNode;
    }

    // Use the embedded tracker node within chainElem
    ChainElem* newTracker = reinterpret_cast<ChainElem*>(&chainElem->embeddedTracker);
    newTracker->nextNode = tracker->nextNode; //TODO: Verify newTracker->trackerJobNode = chainElem is set elsewhere?
    if (tracker->nextNode != nullptr)
        tracker->nextNode->trackerPrevNode = newTracker;

    tracker->nextNode = newTracker;
    newTracker->trackerPrevNode = tracker;

    g_supervisor.leaveCriticalSection(0);
    return registerCallbackResult;
}

int Chain::runCalcChain()
{
    g_supervisor.enterCriticalSection(0);
    ChainElem* trackerChain = this->calcChain.embeddedTracker.trackerNextNode;
    int processedCount = 0;
    bool breakLoop = false;
    
    while (trackerChain && !breakLoop)
    {
        ChainElem* elem = trackerChain->trackerJobNode;
        trackerChain = trackerChain->nextNode;

        if (elem->jobRunDrawChainCallback == nullptr)
        {
            processedCount += 1;
            continue;
        }

        if ((reinterpret_cast<uintptr_t>(elem->nextNode) & 2) == 0)
        {
            processedCount += 1;
            continue;
        }

        if (this->timeToRemove != 0)
        {
            if (elem->runCalcChainCallback)
                (*elem->runCalcChainCallback)(elem->args);
            processedCount += 1;
            continue;
        }

        g_supervisor.leaveCriticalSection(0);
        ChainCallbackResult callbackResult = (*elem->jobRunDrawChainCallback)(elem->args);
        g_supervisor.enterCriticalSection(0);

        switch (callbackResult)
        {
        case CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB:
            cut(elem);
            processedCount += 1;
            break;
        case CHAIN_CALLBACK_RESULT_EXECUTE_AGAIN:
            if ((reinterpret_cast<uintptr_t>(elem->nextNode) & 2) == 0)
            {
                processedCount += 1;
            }
            break;
        case CHAIN_CALLBACK_RESULT_BREAK:
            processedCount = 1;
            breakLoop = true;
            break;
        case CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS:
            processedCount = 0;
            breakLoop = true;
            break;
        case CHAIN_CALLBACK_RESULT_EXIT_GAME_ERROR:
            processedCount = -1;
            breakLoop = true;
            break;
        case CHAIN_CALLBACK_RESULT_RESTART_FROM_FIRST_JOB:
            trackerChain = this->calcChain.embeddedTracker.trackerNextNode;
            processedCount = 0;
            break;
        default:
            processedCount += 1;
            break;
        }
    }
    g_supervisor.leaveCriticalSection(0);
    return processedCount;
}

int Chain::runDrawChain()
{
    g_supervisor.enterCriticalSection(0);
    ChainElem* element = this->drawChain.embeddedTracker.trackerNextNode;
    int processedCount = 0;
    bool breakLoop = false;

    while (element && !breakLoop)
    {
        ChainElem* startingElem = element->trackerJobNode;
        element = element->nextNode;

        if (!startingElem->jobRunDrawChainCallback)
        {
            processedCount += 1;
            continue;
        }

        if ((reinterpret_cast<uintptr_t>(startingElem->nextNode) & 2) == 0)
        {
            processedCount += 1;
            continue;
        }

        g_supervisor.leaveCriticalSection(0);
        ChainCallbackResult callbackResult = (*startingElem->jobRunDrawChainCallback)(startingElem->args);
        g_supervisor.enterCriticalSection(0);

        switch (callbackResult)
        {
        case CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB:
            cut(startingElem);
            processedCount += 1;
            break;
        case CHAIN_CALLBACK_RESULT_EXECUTE_AGAIN:
            if ((reinterpret_cast<uintptr_t>(startingElem->nextNode) & 2) == 0)
                processedCount += 1;
            break;
        case CHAIN_CALLBACK_RESULT_BREAK:
            processedCount = 1;
            breakLoop = true;
            break;
        case CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS:
            processedCount = 0;
            breakLoop = true;
            break;
        case CHAIN_CALLBACK_RESULT_EXIT_GAME_ERROR:
            processedCount = -1;
            breakLoop = true;
            break;
        default:
            processedCount += 1;
            break;
        }
    }
    g_supervisor.leaveCriticalSection(0);
    return processedCount;
}