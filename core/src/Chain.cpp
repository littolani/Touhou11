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

void Chain::release(Chain* This)
{
    //Supervisor::closeThread(&g_supervisor.loadingThread);
    This->timeToRemove = 1;
    runCalcChain(This);
    releaseSingleChain(This, &This->calcChain);
    releaseSingleChain(This, &This->drawChain);
    This->drawChain.jobRunDrawChainCallback = nullptr;
    This->drawChain.registerChainCallback = nullptr;
    This->drawChain.runCalcChainCallback = nullptr;
    This->calcChain.jobRunDrawChainCallback = nullptr;
    This->calcChain.registerChainCallback = nullptr;
    This->calcChain.runCalcChainCallback = nullptr;
}

void Chain::releaseSingleChain(Chain* This, ChainElem* root)
{
    ChainElem* elem = (root->embeddedTracker).trackerNextNode;
    while (elem != nullptr)
    {
        ChainElem* branch = elem->trackerJobNode;
        branch = branch->nextNode;
        if (branch != nullptr)
        {
            g_supervisor.enterCriticalSection(0);
            cut(This, branch);
            g_supervisor.leaveCriticalSection(0);
        }
    }
}

void Chain::cut(Chain* This, ChainElem* elementToRemove)
{
    printf("Chain cut\n");
    if (elementToRemove == nullptr)
        return;

    // Search calcChain for the tracker node pointing to elementToRemove
    ChainElem* tracker = reinterpret_cast<ChainElem*>(&This->calcChain.embeddedTracker);
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
    tracker = reinterpret_cast<ChainElem*>(&This->drawChain.embeddedTracker);
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
        game_free(elementToRemove);
    }
}

int Chain::registerCalcChain(Chain* This, ChainElem* chainElem, int priority)
{
    int registerCallbackResult = 0;
    if (chainElem->registerChainCallback != nullptr) {
        registerCallbackResult = (*chainElem->registerChainCallback)(chainElem->args);
        chainElem->registerChainCallback = nullptr;
    }
    g_supervisor.enterCriticalSection(0);

    chainElem->jobPriority = priority;
    //ChainElem* tracker = &g_chain->calcChain;  // Sentinel node
    ChainElem* tracker = &This->calcChain;  // Sentinel node
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

// 0x456c10
int Chain::registerDrawChain(ChainElem* chainElem, int priority)
{
    int registerResult = 0;

    if (chainElem->registerChainCallback)
    {
        registerResult = chainElem->registerChainCallback(chainElem->args);
        chainElem->registerChainCallback = nullptr;
    }

    g_supervisor.enterCriticalSection(0);
    chainElem->jobPriority = priority;
    TrackerNode* head = reinterpret_cast<TrackerNode*>(&g_chain->drawChain.embeddedTracker);
    TrackerNode* current = head;
    TrackerNode* nextNode = current->next;

    while (nextNode != nullptr)
    {
        ChainElem* job = nextNode->ownerJob;
        if (job->jobPriority >= priority)
            break;

        current = nextNode;
        nextNode = current->next;
    }

    TrackerNode* newTracker = reinterpret_cast<TrackerNode*>(&chainElem->embeddedTracker);
    newTracker->next = nextNode;

    if (nextNode != nullptr)
        nextNode->prev = newTracker; // Link next -> new (if next exists)

    current->next = newTracker; // Link current -> new   
    newTracker->prev = current; // Link new -> current
    g_supervisor.leaveCriticalSection(0);
    return registerResult;
}

int Chain::runCalcChain(Chain* This)
{
    g_supervisor.enterCriticalSection(0);
    ChainElem* trackerChain = This->calcChain.embeddedTracker.trackerNextNode;
    int processedCount = 0;

    while (trackerChain)
    {
        ChainElem* startingElem = trackerChain->trackerJobNode;
        ChainElem* nextTrackerChain = trackerChain->nextNode;
        trackerChain = nextTrackerChain;

        if (!startingElem)
            continue;

        // Check whether the node is a job node
        if ((reinterpret_cast<uintptr_t>(startingElem->nextNode) & 2) != 0)
        {
            bool reExecute = false;
            bool forceContinueOuter = false;

            do {
                reExecute = false;

                if (This->timeToRemove == 0)
                {
                    g_supervisor.leaveCriticalSection(0);
                    ChainCallbackResult result = startingElem->jobRunDrawChainCallback(startingElem->args);
                    g_supervisor.enterCriticalSection(0);

                    switch (result)
                    {
                    case CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB:
                        cut(This, startingElem);
                        processedCount++;
                        forceContinueOuter = true;
                        break;

                    case CHAIN_CALLBACK_RESULT_EXECUTE_AGAIN:
                        if ((reinterpret_cast<uintptr_t>(startingElem->nextNode) & 2) != 0)
                            reExecute = true;
                        break;

                    case CHAIN_CALLBACK_RESULT_BREAK:
                        g_supervisor.leaveCriticalSection(0);
                        return 1;

                    case CHAIN_CALLBACK_RESULT_QUIT_GAME:
                    case CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS:
                        g_supervisor.leaveCriticalSection(0);
                        return 0;

                    case CHAIN_CALLBACK_RESULT_EXIT_GAME_ERROR:
                        g_supervisor.leaveCriticalSection(0);
                        return -1;

                    case CHAIN_CALLBACK_RESULT_RESTART_FROM_FIRST_JOB:
                        trackerChain = This->calcChain.embeddedTracker.trackerNextNode;
                        processedCount = 0;
                        forceContinueOuter = true;
                        break;

                    case CHAIN_CALLBACK_RESULT_CLEANUP_AND_CONTINUE:
                        if (startingElem->runCalcChainCallback) {
                            startingElem->runCalcChainCallback(startingElem->args);
                        }
                        [[fallthrough]];

                    default:
                        break;
                    }
                }
                else
                {
                    // Destruction Path
                    if (startingElem->runCalcChainCallback)
                        startingElem->runCalcChainCallback(startingElem->args);
                }

            } while (reExecute);

            if (forceContinueOuter)
                continue;
        }
        processedCount++;
    }
    g_supervisor.leaveCriticalSection(0);
    return processedCount;
}

int Chain::runDrawChain(void)
{
    int processedCount = 0;
    Chain* pChain = g_chain;

    g_supervisor.enterCriticalSection(0);
    ChainElem* element = pChain->drawChain.embeddedTracker.trackerNextNode;

    while (element != nullptr)
    {
        // Cache the current job and the next element in the list
        ChainElem* startingElem = element->trackerJobNode;
        ChainElem* nextElement = element->nextNode;

        if (startingElem->trackerPrevNode != nullptr && (reinterpret_cast<uintptr_t>(startingElem->nextNode) & 2))
        {
            bool executeAgain;
            do {
                executeAgain = false;

                g_supervisor.leaveCriticalSection(0);
                ChainCallbackResult result = startingElem->jobRunDrawChainCallback(startingElem->args);
                g_supervisor.enterCriticalSection(0);

                switch (result)
                {
                case CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB:
                    cut(pChain, startingElem);
                    break;

                case CHAIN_CALLBACK_RESULT_EXECUTE_AGAIN:
                    if (reinterpret_cast<uintptr_t>(startingElem->nextNode) & 2)
                        executeAgain = true;
                    break;

                case CHAIN_CALLBACK_RESULT_BREAK:
                    g_supervisor.leaveCriticalSection(0);
                    return 1;

                case CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS:
                    g_supervisor.leaveCriticalSection(0);
                    return 0;

                case CHAIN_CALLBACK_RESULT_EXIT_GAME_ERROR:
                    g_supervisor.leaveCriticalSection(0);
                    return -1;

                default:
                    break;
                }
            } while (executeAgain);
        }

        processedCount++;
        element = nextElement;
    }

    g_supervisor.leaveCriticalSection(0);
    return processedCount;
}