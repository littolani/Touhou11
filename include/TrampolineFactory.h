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
struct Stack { static constexpr int offset = Offset; };

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
    static constexpr RegCode regCode = R;
};

template<typename T> 
struct IsRegWrapper : std::false_type {};

template<RegCode R>
struct IsRegWrapper<Reg<R>> : std::true_type
{
    static constexpr RegCode regCode = R;
};

template <RegCode R>
struct Returns { static constexpr RegCode regCode = R; };

template<typename T>
struct IsReturnWrapper : std::false_type {};

template<RegCode R>
struct IsReturnWrapper<Returns<R>> : std::true_type
{
    static constexpr RegCode regCode = R;
};

class TrampolineFactory
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

        // Mapping for PUSHAD relative to EBP:
        // EAX (0) is at [EBP - 8]
        // ...
        // EDI (7) is at [EBP - 36]
        int8_t offset = -8 - ((id & 0x7) * 4);

        // Handling High Byte registers (AH, CH, DH, BH - IDs 4-7 with size 1)
        // If we want AH (ID 4), we want the EAX slot (ID 0).
        bool isHighByte = (size == 1 && id >= 4);
        if (isHighByte) offset = -8 - ((id - 4) * 4);

        if (size == 4)
        {
            // PUSH [EBP+Offset]
            addByte(0xFF); addByte(0x75); addByte((uint8_t)offset);
        }
        else
        {
            // MOV EAX, [EBP + Offset]
            addByte(0x8B); addByte(0x45); addByte((uint8_t)offset);

            // Shift if High Byte (AH/BH/etc)
            if (isHighByte) {
                // SHR EAX, 8
                addByte(0xC1); addByte(0xE8); addByte(0x08);
            }

            // Mask to size (standard C++ promotion)
            if (size == 1) {
                // AND EAX, 0xFF 
                addByte(0x25); addDword(0x000000FF);
            }
            else if (size == 2) {
                // AND EAX, 0xFFFF
                addByte(0x25); addDword(0x0000FFFF);
            }

            // PUSH EAX
            addByte(0x50);
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
        if (mem) memcpy(mem, code.data(), code.size());

        // TELL THE CPU WE WROTE NEW CODE
        FlushInstructionCache(GetCurrentProcess(), mem, code.size());

        return mem;
    }

    size_t size() const { return code.size(); }

    // Push All Double-words
    void pushad() { addByte(0x60); }

    // Pop All Double-words
    void popad() { addByte(0x61); }

    // Push EFLAGS register
    void pushfd() { addByte(0x9C); }

    // Pop EFLAGS register
    void popfd() { addByte(0x9D); }

    // Overwrite a register saved by PUSHAD with the current value of EAX.
    // This allows a C++ return value (in EAX) to survive the POPAD instruction.
    void overwriteSavedReg(RegCode targetReg)
    {
        // PUSHAD Order (Top to Bottom / Low Addr to High Addr):
        // EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
        // Offsets from ESP: 0, 4, 8, 12, 16, 20, 24, 28

        uint8_t id = getId(targetReg);
        // Map RegID to PUSHAD offset. 
        // RegIDs: AX=0, CX=1, DX=2, BX=3, SP=4, BP=5, SI=6, DI=7
        // We need: 0->28, 1->24, 2->20, 3->16, 5->8, 6->4, 7->0
        // Formula: (7 - (id & 7)) * 4

        uint8_t offset = (7 - (id & 0x7)) * 4;

        // Instruction: MOV [ESP + offset], EAX
        addByte(0x89);
        addByte(0x44);
        addByte(0x24);
        addByte(offset);
    }
};

template<typename First, typename... Rest>
void ProcessArgsReverse(TrampolineFactory& e)
{
    // If this is a Return wrapper, skip it during argument processing
    if constexpr (IsReturnWrapper<First>::value)
    {
        if constexpr (sizeof...(Rest) > 0)
            ProcessArgsReverse<Rest...>(e);
        return;
    }

    if constexpr (sizeof...(Rest) > 0) // Recurse first (Right-to-Left processing)
        ProcessArgsReverse<Rest...>(e);

    using T = First;
    if constexpr (IsRegWrapper<T>::value) // Register
        e.pushReg(IsRegWrapper<T>::regCode);

    else if constexpr (!IsRegWrapper<T>::value && !IsReturnWrapper<T>::value)  // Only other case is Stack
        e.pushStack(T::offset + 4); // +4 to account for for ebp push
}

template <typename... Args>
void* CreateLtoThunk(void* targetFunction, int retPopSize)
{
    TrampolineFactory e;

    // Prologue
    e.addByte(0x55);    // push ebp
    e.addWord(0xEC8B);  // mov ebp, esp
    e.addByte(0x9C);    // pushfd
    e.addByte(0x60);    // pushad

    // Process arguments
    ProcessArgsReverse<Args...>(e);

    // Call Target
    e.addByte(0xB8);
    e.addDword((uint32_t)targetFunction);
    e.addWord(0xD0FF); // call eax

    // Cleanup Hook Arguments
    constexpr int realArgCount = (sizeof...(Args)) - (IsReturnWrapper<Args>::value + ... + 0);
    uint8_t stackCleanup = realArgCount * 4;

    if (stackCleanup > 0)
    {
        e.addByte(0x83); e.addByte(0xC4); // add esp, X
        e.addByte(stackCleanup);
    }

    // Handle Return Value
    using First = std::tuple_element_t<0, std::tuple<Args..., void>>;
    if constexpr (IsReturnWrapper<First>::value)
    {
        constexpr RegCode r = First::regCode;

        // Only overwrite if it's a GP register (EAX, ECX, etc)
        if (static_cast<uint32_t>(r) & 0x0400)
            e.overwriteSavedReg(r);
    }

    // Epilogue
    e.addByte(0x61); // popad
    e.addByte(0x9D); // popfd
    e.addByte(0x5D); // pop ebp

    // ret n
    if (retPopSize > 0)
    {
        e.addByte(0xC2);
        e.addWord((uint16_t)retPopSize);
    }
    // ret
    else
        e.addByte(0xC3);

    return e.finalize();
}

using AL  = Reg<RegCode::AL>;  using CL  = Reg<RegCode::CL>;  using DL  = Reg<RegCode::DL>;  using BL  = Reg<RegCode::BL>;
using AH  = Reg<RegCode::AH>;  using CH  = Reg<RegCode::CH>;  using DH  = Reg<RegCode::DH>;  using BH  = Reg<RegCode::BH>;
using AX  = Reg<RegCode::AX>;  using CX  = Reg<RegCode::CX>;  using DX  = Reg<RegCode::DX>;  using BX  = Reg<RegCode::BX>;
using SP  = Reg<RegCode::SP>;  using BP  = Reg<RegCode::BP>;  using SI  = Reg<RegCode::SI>;  using DI  = Reg<RegCode::DI>;
using EAX = Reg<RegCode::EAX>; using ECX = Reg<RegCode::ECX>; using EDX = Reg<RegCode::EDX>; using EBX = Reg<RegCode::EBX>;
using ESP = Reg<RegCode::ESP>; using EBP = Reg<RegCode::EBP>; using ESI = Reg<RegCode::ESI>; using EDI = Reg<RegCode::EDI>;
