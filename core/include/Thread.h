#pragma once
#include "Chireiden.h"
#include <thread>

struct Thread
{
    void* vtable;
    HANDLE handle;
    DWORD threadId;
    uint32_t u0;
    uint32_t u1;
    uint32_t u2;
    void* initializationJobCallback;

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
