#include "Chireiden.h"
#include "Globals.h"
#include "Lzss.h"

void BitWriter::writeBit(int bit)
{
    m_bitBuffer = (m_bitBuffer << 1) | (bit & 1);
    m_bitCount++;
    if (m_bitCount == 8)
    {
        m_buffer.push_back(m_bitBuffer);
        m_bitBuffer = 0;
        m_bitCount = 0;
    }
}

void BitWriter::writeBits(int value, int numBits)
{
    for (int i = numBits - 1; i >= 0; i--)
        writeBit((value >> i) & 1);
}

void BitWriter::flush()
{
    if (m_bitCount > 0)
    {
        m_bitBuffer <<= (8 - m_bitCount);
        m_buffer.push_back(m_bitBuffer);
        m_bitBuffer = 0;
        m_bitCount = 0;
    }
}

size_t BitWriter::getBytesWritten() const
{
    return m_buffer.size();
}

const byte* BitWriter::getBuffer() const
{
    return m_buffer.data();
}

void BitReader::shiftBitBuffer()
{
    m_bitBuffer >>= 1;
}

byte BitReader::getBitBuffer()
{
    return m_bitBuffer;
}

int BitReader::readBit()
{
    if (m_bitBuffer == 0x80)
    {
        if ((int)(m_ptr - m_data) < m_size)
        {
            m_currentByte = *m_ptr;
            m_ptr++;
        }
        else
            m_currentByte = 0;
    }

    int bit = (m_currentByte & m_bitBuffer) ? 1 : 0;

    m_bitBuffer >>= 1;
    if (m_bitBuffer == 0)
        m_bitBuffer = 0x80;

    return bit;
}

// Reads numBits, building the value MSB to LSB
int BitReader::readBits(int numBits) {
    int value = 0;
    for (int i = 0; i < numBits; i++)
        value = (value << 1) | readBit();

    return value;
}

byte* Lzss::decompress(byte* in, int compressedSize, byte* out, size_t decompressedSize)
{
    if (out == nullptr)
    {
        out = (byte*)game_malloc(decompressedSize);
        if (out == nullptr)
            return nullptr;
    }

    BitReader reader(in, compressedSize);
    byte* writePtr = out;
    uint32_t ringBufferIndex = 1;
    byte* outEnd = out + decompressedSize;

    while (true)
    {
        int controlBit = reader.readBit();

        if (controlBit == 1)
        {
            byte literal = (byte)reader.readBits(8);
            if (writePtr >= outEnd)
                break;

            *writePtr = literal;
            writePtr++;

            // Update Dictionary
            g_lzssDict[ringBufferIndex] = literal;
            ringBufferIndex = (ringBufferIndex + 1) & 0x1fff;
        }
        else
        {
            int matchOffset = reader.readBits(13);

            // Offset 0 indicates End of Stream
            if (matchOffset == 0)
                break;

            int lengthBits = reader.readBits(4);
            int copyCount = lengthBits + 3;

            for (int i = 0; i < copyCount; i++)
            {
                if (writePtr >= outEnd)
                    break;

                int srcIndex = (matchOffset + i) & 0x1fff;
                byte val = g_lzssDict[srcIndex];

                *writePtr = val;
                writePtr++;

                g_lzssDict[ringBufferIndex] = val;
                ringBufferIndex = (ringBufferIndex + 1) & 0x1fff;
            }
        }
    }
    return out;
}

#define LZSS_OFFSET_BITS 13
#define LZSS_LENGTH_BITS 4
#define LZSS_DICTSIZE (1 << LZSS_OFFSET_BITS)
#define LZSS_LOOKAHEAD_SIZE ((1 << LZSS_LENGTH_BITS) + 2)
#define LZSS_DICTSIZE_MASK (LZSS_DICTSIZE - 1)
#define LZSS_DICTPOS_MOD(pos, amount) ((pos + amount) & LZSS_DICTSIZE_MASK)

LPBYTE Lzss::compress(LPBYTE in, int uncompressedSize, int* compressedSize)
{
    printf("Called compress\n");

    if (!in || uncompressedSize < 0)
        return nullptr;

    BitWriter writer;
    int lookAheadBytes = 0;
    int dictValue = 0;
    int matchLength = 0;
    int matchOffset = 0;
    LPBYTE inCursor = in;
    uint32_t dictHead = 1;

    initEncoderState();

    int i;
    for (i = 0; i < LZSS_LOOKAHEAD_SIZE; i++)
    {
        if ((int)(inCursor - in) >= uncompressedSize)
            dictValue = -1; // EOF
        else
            dictValue = *inCursor++;

        if (dictValue == -1)
            break;

        g_lzssDict[dictHead + i] = (byte)dictValue;
    }
    lookAheadBytes = i;

    initTree(dictHead);

    while (lookAheadBytes > 0)
    {
        // matchLength is updated by AddString inside the update loop below,
        // but for the first iteration or if lookahead shrinks, clamp it.
        if (matchLength > lookAheadBytes)
            matchLength = lookAheadBytes;

        // Matches must be > 2 bytes to be worth the bit cost
        if (matchLength <= 2)
        {
            writer.writeBit(1);

            byte literalByte = g_lzssDict[dictHead];
            writer.writeBits(literalByte, 8);

            matchLength = 1; // We consumed 1 byte
        }
        else
        {

            writer.writeBit(0);
            writer.writeBits(matchOffset, LZSS_OFFSET_BITS);
            writer.writeBits(matchLength - 3, LZSS_LENGTH_BITS);
        }

        int bytesProcessed = matchLength;
        for (i = 0; i < bytesProcessed; i++)
        {
            // Remove the old entry at current position
            deleteString(LZSS_DICTPOS_MOD(dictHead, LZSS_LOOKAHEAD_SIZE));

            // Read next byte from input
            if ((int)(inCursor - in) >= uncompressedSize)
                dictValue = -1;
            else
                dictValue = *inCursor++;

            // Handle buffer refill / EOF
            if (dictValue == -1)
                lookAheadBytes--;
            else
                g_lzssDict[LZSS_DICTPOS_MOD(dictHead, LZSS_LOOKAHEAD_SIZE)] = (byte)dictValue;

            // Advance ring buffer head
            dictHead = LZSS_DICTPOS_MOD(dictHead, 1);

            // Find match for the *next* iteration
            if (lookAheadBytes != 0)
                matchLength = addString(dictHead, &matchOffset);
        }
    }

    
    writer.writeBit(0); // Bit 0: Match Flag
    writer.writeBits(0, LZSS_OFFSET_BITS); // Offset 0: Indicates EOS
    writer.flush();

    size_t finalSize = writer.getBytesWritten();
    *compressedSize = (int)finalSize;

    LPBYTE out = (LPBYTE)game_malloc(finalSize);
    if (out == nullptr)
        return nullptr;

    memcpy(out, writer.getBuffer(), finalSize);
    return out;
}

void Lzss::initTree(int root)
{
    g_lzssTree[LZSS_DICTSIZE].right = root;
    g_lzssTree[root].parent = LZSS_DICTSIZE;
    g_lzssTree[root].right = 0;
    g_lzssTree[root].left = 0;
}

void Lzss::initEncoderState()
{
    int i;
    for (i = 0; i < LZSS_DICTSIZE; i++)
        g_lzssDict[i] = 0;

    for (i = 0; i < LZSS_DICTSIZE + 1; i++)
    {
        g_lzssTree[i].parent = 0;
        g_lzssTree[i].left = 0;
        g_lzssTree[i].right = 0;
    }
}

int Lzss::addString(int newNode, int* matchPosition)
{
    int i;
    int* child;
    int delta;

    if (newNode == 0)
    {
        return 0;
    }

    int testNode = g_lzssTree[LZSS_DICTSIZE].right;
    int matchLength = 0;

    for (;;)
    {
        for (i = 0; i < LZSS_LOOKAHEAD_SIZE; i++)
        {
            delta = g_lzssDict[LZSS_DICTPOS_MOD(newNode, i)] - g_lzssDict[LZSS_DICTPOS_MOD(testNode, i)];

            if (delta != 0)
            {
                break;
            }
        }

        if (i >= matchLength)
        {
            matchLength = i;
            *matchPosition = testNode;

            if (matchLength >= LZSS_LOOKAHEAD_SIZE)
            {
                replaceNode(testNode, newNode);
                return matchLength;
            }
        }

        if (delta >= 0)
            child = &g_lzssTree[testNode].right;
        else
            child = &g_lzssTree[testNode].left;

        if (*child == 0)
        {
            *child = newNode;
            g_lzssTree[newNode].parent = testNode;
            g_lzssTree[newNode].right = 0;
            g_lzssTree[newNode].left = 0;
            return matchLength;
        }

        testNode = *child;
    }
}

void Lzss::deleteString(int p)
{
    if (g_lzssTree[p].parent == 0)
        return;

    if (g_lzssTree[p].right == 0)
        contractNode(p, g_lzssTree[p].left);

    else if (g_lzssTree[p].left == 0)
        contractNode(p, g_lzssTree[p].right);

    else
    {
        int replacement = findNextNode(p);
        deleteString(replacement);
        replaceNode(p, replacement);
    }
}

void Lzss::contractNode(int oldNode, int newNode)
{
    g_lzssTree[newNode].parent = g_lzssTree[oldNode].parent;

    if (g_lzssTree[g_lzssTree[oldNode].parent].right == oldNode)
        g_lzssTree[g_lzssTree[oldNode].parent].right = newNode;
    else
        g_lzssTree[g_lzssTree[oldNode].parent].left = newNode;

    g_lzssTree[oldNode].parent = 0;
}

void Lzss::replaceNode(int oldNode, int newNode)
{
    int parent = g_lzssTree[oldNode].parent;

    if (g_lzssTree[parent].left == oldNode)
        g_lzssTree[parent].left = newNode;
    else
        g_lzssTree[parent].right = newNode;

    g_lzssTree[newNode] = g_lzssTree[oldNode];
    g_lzssTree[g_lzssTree[newNode].left].parent = newNode;
    g_lzssTree[g_lzssTree[newNode].right].parent = newNode;
    g_lzssTree[oldNode].parent = 0;
}

int Lzss::findNextNode(int node)
{
    int next = g_lzssTree[node].left;
    while (g_lzssTree[next].right != 0)
        next = g_lzssTree[next].right;
    return next;
}