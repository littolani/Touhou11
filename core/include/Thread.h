#pragma once
#include "Chireiden.h"
#include <thread>

struct Thread
{
    void* vtable;
    HANDLE handle;
    uint32_t threadId;
    uint32_t u0;
    uint32_t u1;
    uint32_t u2;
    void* initializationJobCallback;

    Thread(_beginthreadex_proc_type initializationJobCallback)
    {
        this->initializationJobCallback = initializationJobCallback;
        this->u1 = 1;
        this->u0 = 0;
        this->handle = (HANDLE)_beginthreadex(NULL, 0, initializationJobCallback, NULL, 0, &this->threadId);
    }

    static void close(Thread* This)
    {
        DWORD waitState;

        if (This->handle)
        {
            This->u0 = 1;
            This->u1 = 0;
            waitState = WaitForSingleObject(This->handle, 200);
            while (waitState == 0x102)
            {
                This->u0 = 1;
                This->u1 = 0;
                Sleep(1);
                waitState = WaitForSingleObject(This->handle, 200);
            }
            CloseHandle(This->handle);
            This->handle = NULL;
            This->initializationJobCallback = NULL;
        }
        return;
    }
};
ASSERT_SIZE(Thread, 0x1c);
