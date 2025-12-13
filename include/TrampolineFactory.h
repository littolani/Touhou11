#pragma once
#include <windows.h>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <iostream>

inline void installHook(DWORD targetAddress, void* hookFunction)
{
    DWORD relativeJump = (DWORD)hookFunction - targetAddress - 5;
    DWORD oldProtect;
    VirtualProtect((void*)targetAddress, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(BYTE*)targetAddress = 0xE9; // JMP opcode
    *(DWORD*)(targetAddress + 1) = relativeJump;
    VirtualProtect((void*)targetAddress, 5, oldProtect, &oldProtect);
}

// Represents a value located on the stack at [EBP + Offset]
template <int Offset>
struct Stack { static constexpr int val = Offset; };

enum class RegCode : uint32_t
{
    AL  = 0x0100, CL  = 0x0101, DL  = 0x0102, BL  = 0x0103,
    AH  = 0x0104, CH  = 0x0105, DH  = 0x0106, BH  = 0x0107,
    AX  = 0x0200, CX  = 0x0201, DX  = 0x0202, BX  = 0x0203,
    SP  = 0x0204, BP  = 0x0205, SI  = 0x0206, DI  = 0x0207,
    EAX = 0x0400, ECX = 0x0401, EDX = 0x0402, EBX = 0x0403,
    ESP = 0x0404, EBP = 0x0405, ESI = 0x0406, EDI = 0x0407
};

constexpr uint8_t getId(RegCode r)
{
    return static_cast<uint32_t>(r) & 0xf;
}

constexpr uint8_t getSize(RegCode r)
{
    return (static_cast<uint32_t>(r) >> 8) & 0xff;
}

// Represents a value in a CPU register (8, 16, or 32-bit)
template <RegCode R>
struct Reg
{
    static constexpr RegCode value = R;
};

// Type trait to detect if T is a Register wrapper
template<typename T> 
struct IsRegWrapper : std::false_type {};

template<RegCode R>
struct IsRegWrapper<Reg<R>> : std::true_type
{
    static constexpr RegCode id = R;
};

class ThunkEmitter
{
    std::vector<uint8_t> code;

public:
    void addByte(uint8_t b)
    {
        code.push_back(b);
    }

    void addWord(uint16_t w)
    {
        code.insert(code.end(),
        {
            static_cast<uint8_t>(w & 0xFF),
            static_cast<uint8_t>(w >> 8)
        });
    }

    void addDword(uint32_t d)
    {
        code.insert(code.end(),
        {
            static_cast<uint8_t>(d & 0xFF),
            static_cast<uint8_t>((d >> 8) & 0xFF),
            static_cast<uint8_t>((d >> 16) & 0xFF),
            static_cast<uint8_t>(d >> 24)
        });
    }

    // Emit: push [ebp + offset]
    void pushStack(int offset)
    {
        // Opcode FF 75 XX (assuming offset fits in 1 byte signed)
        addByte(0xFF);
        addByte(0x75);
        addByte((uint8_t)offset);
    }

    void pushReg(RegCode r)
    {
        uint8_t id = getId(r);
        uint8_t size = getSize(r);

        if (size == 4)
        {
            // Standard 32-bit PUSH: 0x50 + ID
            addByte(0x50 + id);
        }
        else if (size == 2)
        {
            // 16-bit: Zero-Extend to ECX, then Push ECX
            // MOVZX ECX, r16 -> 0F B7 /r (Dest=ECX=001)
            // ModRM: 11(Reg) 001(Dest) XXX(Src)
            // 0xC8 + SrcID
            addByte(0x0F);
            addByte(0xB7);
            addByte(0xC8 + id);
            addByte(0x51); // push ecx
        }
        else if (size == 1)
        {
            // 8-bit: Zero-Extend to ECX, then Push ECX
            // MOVZX ECX, r8 -> 0F B6 /r (Dest=ECX=001)
            // ModRM: 11(Reg) 001(Dest) XXX(Src)
            // 0xC8 + SrcID (Note: AH/CH/DH/BH have IDs 4-7, which works perfectly here)
            addByte(0x0F);
            addByte(0xB6);
            addByte(0xC8 + id);
            addByte(0x51); // push ecx
        }
    }

    void call(void* target, void* currentIP)
    {
        addByte(0xE8);
        uint32_t rel = (uint32_t)target - ((uint32_t)currentIP + 5);
        addDword(rel);
    }

    void* finalize()
    {
        void* mem = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        memcpy(mem, code.data(), code.size());

        // TELL THE CPU WE WROTE NEW CODE
        FlushInstructionCache(GetCurrentProcess(), mem, code.size());

        return mem;
    }

    size_t size() const { return code.size(); }
};

template<typename First, typename... Rest>
void ProcessArgsReverse(ThunkEmitter& e)
{
    if constexpr (sizeof...(Rest) > 0) // Recurse first (Right-to-Left processing)
        ProcessArgsReverse<Rest...>(e);

    using T = First;
    if constexpr (IsRegWrapper<T>::value) // Register
        e.pushReg(IsRegWrapper<T>::id);
    else  // Assume stack
        e.pushStack(T::val + 4); // +4 for ebp push so it looks more normal on the CreateLtoThunk side
}

template <typename... Args>
void* CreateLtoThunk(void* targetFunction, int retPopSize) {
    ThunkEmitter e;

    // 1. Prologue
    e.addByte(0x55); // push ebp
    e.addWord(0xEC8B); // mov ebp, esp

    // 2. Push Arguments (Right-to-Left)
    ProcessArgsReverse<Args...>(e);

    // Call Target
    e.addByte(0xB8); // mov eax, IMM32
    e.addDword((uint32_t)targetFunction);
    e.addWord(0xD0FF); // call eax

    // Cleanup Pushes (Args count * 4)
    uint8_t stackCleanup = sizeof...(Args) * 4;
    if (stackCleanup > 0) {
        e.addByte(0x83); e.addByte(0xC4); // add esp, ...
        e.addByte(stackCleanup);
    }

    // Epilogue
    e.addByte(0x5D); // pop ebp

    // Return
    if (retPopSize > 0) {
        e.addByte(0xC2); // ret n
        e.addWord((uint16_t)retPopSize);
    }
    else
        e.addByte(0xC3); // ret

    return e.finalize();
}

using AL  = Reg<RegCode::AL>;  using CL  = Reg<RegCode::CL>;  using DL  = Reg<RegCode::DL>;  using BL  = Reg<RegCode::BL>;
using AH  = Reg<RegCode::AH>;  using CH  = Reg<RegCode::CH>;  using DH  = Reg<RegCode::DH>;  using BH  = Reg<RegCode::BH>;
using AX  = Reg<RegCode::AX>;  using CX  = Reg<RegCode::CX>;  using DX  = Reg<RegCode::DX>;  using BX  = Reg<RegCode::BX>;
using SP  = Reg<RegCode::SP>;  using BP  = Reg<RegCode::BP>;  using SI  = Reg<RegCode::SI>;  using DI  = Reg<RegCode::DI>;
using EAX = Reg<RegCode::EAX>; using ECX = Reg<RegCode::ECX>; using EDX = Reg<RegCode::EDX>; using EBX = Reg<RegCode::EBX>;
using ESP = Reg<RegCode::ESP>; using EBP = Reg<RegCode::EBP>; using ESI = Reg<RegCode::ESI>; using EDI = Reg<RegCode::EDI>;
