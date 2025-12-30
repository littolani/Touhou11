#pragma once

class BitReader
{
public:
    BitReader(byte* data, int size) : m_data(data), m_size(size), m_ptr(data), m_bitBuffer(0x80) {}
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


struct LzssTreeNode
{
    int parent;
    int left;
    int right;
};

class Lzss
{
public:

    /**
     * 0x4423a0
     * @brief
     * @param  in               Stack[0x4]:4
     * @param  uncompressedSize Stack[0x8]:4
     * @param  compressedSize   Stack[0xc]:4
     * @return byte*            EAX:4
     */
    static LPBYTE compress(LPBYTE in, int uncompressedSize, int* compressedSize);

    /**
     * 0x4426c0
     * @brief 
     * @param  compressedData   Stack[0x4]:4
     * @param  compressedSize   Stack[0x8]:4
     * @param  outBuffer        Stack[0xc]:4
     * @param  decompressedSize EAX:4
     * @return byte*            EAX:4
     */
    static byte* decompress(byte* in, int compressedSize, byte* out, size_t decompressedSize);

    static void initTree(int root);
    static void initEncoderState();
    static int addString(int newNode, int* matchPosition);
    static void deleteString(int p);
    static void contractNode(int oldNode, int newNode);
    static void replaceNode(int oldNode, int newNode);
    static int findNextNode(int node);
};
