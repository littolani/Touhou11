#pragma once
#include "Chireiden.h"
#include "Macros.h"
#include "Globals.h"
#include "Lzss.h"

#if 0
class BitReader
{
public:
    BitReader(byte* data, int size): m_data(data), m_size(size), m_ptr(data), m_bitBuffer(0x80) {}
    int readBit();
    int readBits(int num_bits);
    byte getBitBuffer();
    void shiftBitBuffer();
private:
    byte* m_data;        // Start of compressed data
    int m_size;          // Size of compressed data
    byte* m_ptr;         // Current read position
    byte m_currentByte; // Current byte being processed
    byte m_bitBuffer;   // Bit position within the byte
};

class BitWriter 
{
public:
    BitWriter() : m_bitBuffer(0), m_bitCount(0) {}
    void writeBit(int bit);
    void writeBits(int value, int numBits);
    void flush();
    size_t getBytesWritten() const;
    const byte* getBuffer() const;
private:
    std::vector<byte> m_buffer;
    byte m_bitBuffer;
    int m_bitCount;
};
#endif

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
    byte* readDecompressEntry(LPCSTR filename, byte* outBuffer);
    DWORD getEntryDecompressedSize(LPCSTR filename);
    bool parseHeader(LPCSTR filename);
    PbgArchiveEntry* findEntry(LPCSTR filename);
    PbgArchiveEntry* allocEntries(LPVOID entryBuffer, int count, uint32_t offset);
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

extern PbgArchive g_pbgArchive;
PbgArchive* findMatchingArchive(const char* filename);