#pragma once
#include "Chireiden.h"
#include "Macros.h"
#include "Globals.h"
#include "Lzss.h"

struct DecryptionKey
{
    uint8_t key;
    uint8_t step;
    uint16_t unknown;
    uint32_t block;
    uint32_t limit;
};

class FileUtils
{
public:
    static void decrypt(uint8_t* data, uint32_t size, uint8_t key, uint8_t step, uint32_t block, uint32_t limit);
    static void encrypt(uint8_t* data, uint32_t size, uint8_t key, uint8_t step, uint32_t block, uint32_t limit);
};

class IPbgFile
{
  public:
    IPbgFile() {}
    virtual DWORD open(const char* filename, const char* mode) = 0;
    virtual void close() = 0;
    virtual DWORD read(LPVOID data, DWORD dataLen) = 0;
    virtual bool write(LPVOID data, DWORD dataLen) = 0;
    virtual DWORD tell() = 0;
    virtual DWORD getSize() = 0;
    virtual bool seek(DWORD offset, DWORD seekFrom) = 0;
    virtual ~IPbgFile() {};
};

class CPbgFile : public IPbgFile
{
  public:
    CPbgFile();
    virtual ~CPbgFile();
    virtual DWORD open(const char* filename, const char* mode);
    virtual void close();
    virtual DWORD read(LPVOID data, DWORD dataLen);
    virtual bool write(LPVOID data, DWORD dataLen);
    virtual DWORD tell();
    virtual DWORD getSize();
    virtual bool seek(DWORD offset, DWORD seekFrom);

    virtual HGLOBAL readWholeFile(DWORD maxSize);
    static void getFullFilePath(char* buffer, const char* filename);

    DWORD readInt(int* outData)
    {
        return read(outData, 4);
    }

  protected:
    HANDLE m_handle;

  private:
    DWORD m_access;
};

struct PbgArchiveHeader
{
    uint32_t magicNumber;
    int decompressedSize;
    int fileTableOffset;
    int numEntries;
};
ASSERT_SIZE(PbgArchiveHeader, 0x10);

struct PbgArchiveEntry
{
    PbgArchiveEntry() {}
    ~PbgArchiveEntry() { delete[] filename; }

    char* filename{};
    uint32_t dataOffset{};
    uint32_t decompressedSize{};
    uint32_t unk{};
};
ASSERT_SIZE(PbgArchiveEntry, 0x10);

struct PbgArchive
{
    PbgArchive();
    ~PbgArchive();

    bool load(LPCSTR filename);
    void release();

    /**
     * 0x441760
     * @brief
     * @param  This           Stack[0x4]:4
     * @param  outBuffer      Stack[0x8]:4
     * @param  filename       ECX:4
     * @return byte*          EAX:4
     */
    static byte* readDecompressEntry(PbgArchive* This, byte* outBuffer, LPCSTR filename);

    DWORD getEntryDecompressedSize(LPCSTR filename);
    bool parseHeader(LPCSTR filename);

    /**
     * 0x4418d0
     * @brief
     * @param  This           EAX:4
     * @param  filename       EBX:4
     * @return byte*          EAX:4
     */
    static PbgArchiveEntry* findEntry(PbgArchive* This, LPCSTR filename);

    PbgArchiveEntry* allocEntries(LPVOID entryBuffer, int count, uint32_t offset);
    static PbgArchive* findMatchingArchive(const char* filename);
    LPSTR copyFileName(LPCSTR fileName);
    static int seekPastInt(LPVOID* ptr);
    static LPVOID seekPastString(LPVOID* ptr);

    PbgArchiveEntry* m_entries;
    int m_numEntries;
    char* m_filename;
    CPbgFile* m_fileAbstraction;
};

const DecryptionKey decryptionKeySet[8] =
{
    { 0x1b, 0x37, 0xaa, 0x40,  0x2800 },
    { 0x51, 0xE9, 0xBB, 0x40,  0x3000 },
    { 0xC1, 0x51, 0xCC, 0x80,  0x3200 },
    { 0x03, 0x19, 0xDD, 0x400, 0x7800 },
    { 0xAB, 0xCD, 0xEE, 0x200, 0x2800 },
    { 0x12, 0x34, 0xFF, 0x80,  0x3200 },
    { 0x35, 0x97, 0x11, 0x80,  0x2800 },
    { 0x99, 0x37, 0x77, 0x400, 0x2000 }
};