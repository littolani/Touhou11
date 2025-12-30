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
    printf("Called cut\n");
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

    // 1. Handle Callback
    if (chainElem->registerChainCallback)
    {
        registerResult = chainElem->registerChainCallback(chainElem->args);
        chainElem->registerChainCallback = nullptr;
    }

    // 2. Critical Section Enter
    if (g_supervisor.criticalSectionFlag & 0x8000)
    {
        EnterCriticalSection(&g_supervisor.criticalSections[0]);
        g_supervisor.criticalSectionCounters[0]++;
    }

    // 3. Set Priority
    chainElem->jobPriority = priority;

    // 4. Find Insertion Point
    // The list is sorted Low Priority -> High Priority.
    // We iterate to find the first node where (current->priority >= new_priority) is FALSE?
    // Assembly: cmp new, current; jge break.
    // Stop if (New >= Current). This implies we skip while (New < Current).
    // Wait, typical sorting:
    // List: 10, 20, 30. New: 25.
    // 10: 25 >= 10? True. Break. Insert before 10? No.
    // The assembly compares [EDX] (current) with EBX (new).
    // CMP [EDX], EBX -> Current - New.
    // JGE -> Jump if Current >= New.
    // So we skip as long as Current < New. (List is Low -> High).

    // Start at the head (embeddedTracker of g_chain->drawChain)
    TrackerNode* head = reinterpret_cast<TrackerNode*>(&g_chain->drawChain.embeddedTracker);
    TrackerNode* current = head;
    TrackerNode* nextNode = current->next;

    while (nextNode != nullptr)
    {
        // For a TrackerNode, offset 0x0 is the pointer to the actual ChainElem (owner)
        ChainElem* job = nextNode->ownerJob;

        // Discrepancy Fix: Check if we found a node with priority >= new priority
        // Assembly: cmp [job->priority], priority; jge found_spot
        if (job->jobPriority >= priority)
        {
            break;
        }

        current = nextNode;
        nextNode = current->next;
    }

    // 5. Insert 'chainElem' between 'current' and 'current->next'
    // 'current' is the node BEFORE our new node.

    // Get pointer to the embedded tracker of the new element
    TrackerNode* newTracker = reinterpret_cast<TrackerNode*>(&chainElem->embeddedTracker);

    // Link new -> next
    newTracker->next = nextNode; // can be null

    // Link next -> new (if next exists)
    if (nextNode != nullptr)
        nextNode->prev = newTracker;

    // Link current -> new
    current->next = newTracker;

    // Link new -> current
    newTracker->prev = current;

    // 6. Critical Section Leave
    if (g_supervisor.criticalSectionFlag & 0x8000)
    {
        LeaveCriticalSection(&g_supervisor.criticalSections[0]);
        g_supervisor.criticalSectionCounters[0]--; // Asm adds 0xff, effectively sub 1
    }

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

            do
            {
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
            do
            {
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