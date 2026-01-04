#pragma once

class AnmVm;
struct AnmVmList
{
    AnmVm* entry;
    AnmVmList* next;
    AnmVmList* prev;
};