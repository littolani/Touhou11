#include "Chain.h"
#include "Globals.h"
#include "Supervisor.h"

Chain::Chain()
{
    calcChain.listNode.prevNode = nullptr;
    calcChain.registerChainCallback = nullptr;
    calcChain.priority = 0;
    calcChain.flags &= ~1;
    calcChain.listNode.nextNode = nullptr;
    calcChain.listNode.prevNode = nullptr;
    calcChain.listNode.data = &calcChain;
    drawChain.listNode.prevNode = nullptr;
    drawChain.registerChainCallback = nullptr;
    drawChain.priority = 0;
    drawChain.flags &= ~1;
    drawChain.listNode.nextNode = nullptr;
    drawChain.listNode.prevNode = nullptr;
    drawChain.listNode.data = &drawChain;
    timeToRemove = 0;
}

void Chain::releaseSingleChain(Chain* This, ChainElem* root)
{
    ZUNList<ChainElem*>* start = root->listNode.nextNode;
    while (start != nullptr)
    {
        ChainElem* elem = start->data;
        start = start->nextNode;
        if (elem != nullptr)
        {
            g_supervisor.enterCriticalSection(0);
            cut(This, elem);
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
    ZUNList<ChainElem*>* listNode = &This->calcChain.listNode;
    while (listNode != nullptr)
    {
        if (listNode->data == elementToRemove)
        {
            removeTracker(listNode, elementToRemove);
            return;
        }
        listNode = listNode->nextNode;
    }

    // If not found, search drawChain
    listNode = &This->drawChain.listNode;
    while (listNode != nullptr)
    {
        if (listNode->data == elementToRemove)
        {
            removeTracker(listNode, elementToRemove);
            return;
        }
        listNode = listNode->nextNode;
    }
}

void Chain::removeTracker(ZUNList<ChainElem*>* tracker, ChainElem* elementToRemove)
{
    // Update doubly-linked list pointers to remove tracker node
    if (tracker->prevNode != nullptr)
    {
        if (tracker->nextNode != nullptr)
            tracker->nextNode->prevNode = tracker->prevNode;

        if (tracker->prevNode != nullptr)
            tracker->prevNode->nextNode = tracker->nextNode;

        tracker->nextNode = nullptr;
        tracker->prevNode = nullptr;
    }

    // Cleanup the job node (elementToRemove)
    elementToRemove->runCalcChainCallback = nullptr;
    if (elementToRemove->flags & 1)
    {
        // Heap-allocated job node: clear additional fields and free
        elementToRemove->runCalcChainCallback = nullptr;
        elementToRemove->registerChainCallback = nullptr;
        elementToRemove->cutCallback = nullptr;
        game_free(elementToRemove);
    }
}

int Chain::registerCalcChain(ChainElem* chainElem, int priority)
{
    puts("Registered calc chain\n");
    Chain* This = g_chain;
    int registerCallbackResult = 0;

    // Handle registration callback
    if (chainElem->registerChainCallback != nullptr) {
        registerCallbackResult = chainElem->registerChainCallback(chainElem->args);
        chainElem->registerChainCallback = nullptr;
    }

    g_supervisor.enterCriticalSection(0);

    chainElem->priority = priority;

    // Grab the embedded tracker instead of the base ChainElem
    ZUNList<ChainElem*>* tracker = &This->calcChain.listNode;
    ZUNList<ChainElem*>* trackerNext = tracker->nextNode;

    // Traverse the tracker linked list
    while (trackerNext != nullptr)
    {
        ChainElem* jobNode = trackerNext->data;
        if (jobNode->priority >= priority)
            break;

        tracker = trackerNext;
        trackerNext = trackerNext->nextNode;
    }

    // Link the new tracker into the list
    ZUNList<ChainElem*>* newTracker = &chainElem->listNode;
    newTracker->nextNode = trackerNext;

    if (trackerNext != nullptr)
        trackerNext->prevNode = newTracker;

    tracker->nextNode = newTracker;
    newTracker->prevNode = tracker;

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
    chainElem->priority = priority;
    ZUNList<ChainElem*>* head = &g_chain->drawChain.listNode;
    ZUNList<ChainElem*>* current = head;
    ZUNList<ChainElem*>* nextNode = current->nextNode;

    while (nextNode != nullptr)
    {
        ChainElem* job = nextNode->data;
        if (job->priority >= priority)
            break;

        current = nextNode;
        nextNode = current->nextNode;
    }

    ZUNList<ChainElem*>* newTracker = &chainElem->listNode;
    newTracker->nextNode = nextNode;

    if (nextNode != nullptr)
        nextNode->prevNode = newTracker; // Link next -> new (if next exists)

    current->nextNode = newTracker; // Link current -> new   
    newTracker->prevNode = current; // Link new -> current
    g_supervisor.leaveCriticalSection(0);
    return registerResult;
}

int Chain::runCalcChain(Chain* This)
{
    g_supervisor.enterCriticalSection(0);
    ZUNList<ChainElem*>* trackerChain = This->calcChain.listNode.nextNode;
    int processedCount = 0;

    while (trackerChain)
    {
        ChainElem* startingElem = trackerChain->data;
        ZUNList<ChainElem*>* nextTrackerChain = trackerChain->nextNode;
        trackerChain = nextTrackerChain;

        if (!startingElem)
            continue;

        // Check whether the node is a running job
        if (startingElem->flags & 2)
        {
            bool reExecute = false;
            bool forceContinueOuter = false;

            do {
                reExecute = false;

                if (This->timeToRemove == 0)
                {
                    g_supervisor.leaveCriticalSection(0);
                    ChainCallbackResult result = startingElem->runCalcChainCallback(startingElem->args);
                    g_supervisor.enterCriticalSection(0);

                    switch (result)
                    {
                    case ContinueAndRemoveJob:
                        cut(This, startingElem);
                        processedCount++;
                        forceContinueOuter = true;
                        break;

                    case ExecuteAgain:
                        if (startingElem->flags & 2)
                            reExecute = true;
                        break;

                    case Break:
                        g_supervisor.leaveCriticalSection(0);
                        return 1;

                    case QuitGame:
                    case ExitGameSuccess:
                        g_supervisor.leaveCriticalSection(0);
                        return 0;

                    case ExitGameError:
                        g_supervisor.leaveCriticalSection(0);
                        return -1;

                    case RestartFromFirstJob:
                        trackerChain = This->calcChain.listNode.nextNode;
                        processedCount = 0;
                        forceContinueOuter = true;
                        break;

                    case CleanupAndContinue:
                        if (startingElem->cutCallback) {
                            startingElem->cutCallback(startingElem->args);
                        }
                        [[fallthrough]];

                    default:
                        break;
                    }
                }
                else
                {
                    // Destruction Path
                    if (startingElem->cutCallback)
                        startingElem->cutCallback(startingElem->args);
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

int Chain::runDrawChain()
{
    int processedCount = 0;
    Chain* pChain = g_chain;

    g_supervisor.enterCriticalSection(0);
    ZUNList<ChainElem*>* element = pChain->drawChain.listNode.nextNode;

    while (element != nullptr)
    {
        // Cache the current job and the next element in the list
        ChainElem* startingElem = element->data;
        ZUNList<ChainElem*>* nextElement = element->nextNode;

        if (startingElem->listNode.prevNode != nullptr && startingElem->flags & 2)
        {
            bool executeAgain;
            do {
                executeAgain = false;

                g_supervisor.leaveCriticalSection(0);
                ChainCallbackResult result = startingElem->runCalcChainCallback(startingElem->args);
                g_supervisor.enterCriticalSection(0);

                switch (result)
                {
                case ContinueAndRemoveJob:
                    cut(pChain, startingElem);
                    break;

                case ExecuteAgain:
                    if (startingElem->flags & 2)
                        executeAgain = true;
                    break;

                case Break:
                    g_supervisor.leaveCriticalSection(0);
                    return 1;

                case ExitGameSuccess:
                    g_supervisor.leaveCriticalSection(0);
                    return 0;

                case ExitGameError:
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