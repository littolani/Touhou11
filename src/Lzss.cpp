#include "Chireiden.h"
#include "Lzss.h"

#define LZSS_LOOKAHEAD_SIZE ((1 << LZSS_LENGTH_BITS) + 2)
#define LZSS_DICTSIZE_MASK (LZSS_DICTSIZE - 1)
#define LZSS_DICTPOS_MOD(pos, amount) ((pos + amount) & LZSS_DICTSIZE_MASK)

Lzss::TreeNode Lzss::m_Tree[LZSS_DICTSIZE + 1];
uint8_t Lzss::m_Dict[LZSS_DICTSIZE];

#define ENC_NEXT_BIT()                                                                                                 \
inBitMask >>= 1;                                                                                                   \
if (inBitMask == 0)                                                                                                \
{                                                                                                                  \
    *outCursor++ = currByte;                                                                                       \
    checksum += currByte;                                                                                          \
    currByte = 0;                                                                                                  \
    inBitMask = 0x80;                                                                                              \
}

#define ENC_WRITE_FLAG_BIT(bit)                                                                                        \
if (bit)                                                                                                           \
{                                                                                                                  \
    currByte |= inBitMask;                                                                                         \
}                                                                                                                  \
ENC_NEXT_BIT();

#define ENC_WRITE_BITS(bitCount, condition)                                                                            \
bitfieldMask = 0x1 << (bitCount - 1);                                                                              \
while (bitfieldMask != 0)                                                                                          \
{                                                                                                                  \
    if (condition)                                                                                                 \
    {                                                                                                              \
        currByte |= inBitMask;                                                                                     \
    }                                                                                                              \
    ENC_NEXT_BIT();                                                                                                \
    bitfieldMask >>= 1;                                                                                            \
}

LPBYTE Lzss::compress(LPBYTE in, int uncompressedSize, int* compressedSize)
{
    int i;
    int bytesToCopyToDict;
    int lookAheadBytes;
    int dictValue;
    uint32_t bitfieldMask;

    uint8_t inBitMask = 0x80;
    uint32_t currByte = 0;
    uint32_t checksum = 0;

    LPBYTE out = (LPBYTE)GlobalAlloc(GMEM_FIXED, uncompressedSize * 2);
    if (out == NULL)
    {
        return NULL;
    }

    LPBYTE inCursor = in;
    LPBYTE outCursor = out;
    *compressedSize = 0;

    InitEncoderState();

    uint32_t dictHead = 1;
    for (i = 0; i < LZSS_LOOKAHEAD_SIZE; i++)
    {
        if (inCursor - in >= uncompressedSize)
        {
            dictValue = -1;
        }
        else
        {
            dictValue = *inCursor++;
        }

        if (dictValue == -1)
        {
            break;
        }

        m_Dict[dictHead + i] = dictValue;
    }

    lookAheadBytes = i;
    InitTree(dictHead);
    int matchLength = 0;
    int matchOffset = 0;

    while (lookAheadBytes > 0)
    {
        if (matchLength > lookAheadBytes)
        {
            matchLength = lookAheadBytes;
        }

        if (matchLength <= 2)
        {
            bytesToCopyToDict = 1;

            ENC_WRITE_FLAG_BIT(1);
            ENC_WRITE_BITS(8, (bitfieldMask & m_Dict[dictHead]) != 0);
        }
        else
        {
            ENC_WRITE_FLAG_BIT(0);
            ENC_WRITE_BITS(LZSS_OFFSET_BITS, (bitfieldMask & matchOffset) != 0);
            ENC_WRITE_BITS(LZSS_LENGTH_BITS, (bitfieldMask & (matchLength - 3)) != 0);

            bytesToCopyToDict = matchLength;
        }

        for (i = 0; i < bytesToCopyToDict; i++)
        {
            DeleteString(LZSS_DICTPOS_MOD(dictHead, LZSS_LOOKAHEAD_SIZE));

            if (inCursor - in >= uncompressedSize)
            {
                dictValue = -1;
            }
            else
            {
                dictValue = *inCursor++;
            }

            if (dictValue == -1)
            {
                lookAheadBytes--;
            }
            else
            {
                m_Dict[LZSS_DICTPOS_MOD(dictHead, LZSS_LOOKAHEAD_SIZE)] = dictValue;
            }

            dictHead = LZSS_DICTPOS_MOD(dictHead, 1);

            if (lookAheadBytes != 0)
            {
                matchLength = AddString(dictHead, &matchOffset);
            }
        }
    }

    ENC_WRITE_FLAG_BIT(0);
    ENC_WRITE_BITS(LZSS_OFFSET_BITS, FALSE);

    *compressedSize = outCursor - out;
    return out;
}

#define DEC_NEXT_BIT()                                                                                                 \
inBitMask >>= 1;                                                                                                   \
if (inBitMask == 0)                                                                                                \
{                                                                                                                  \
    inBitMask = 0x80;                                                                                              \
}

#define DEC_WRITE_BYTE(data)                                                                                           \
*outCursor++ = data;                                                                                               \
m_Dict[dictHead] = data;                                                                                           \
dictHead = LZSS_DICTPOS_MOD(dictHead, 1);

#define DEC_HANDLE_FETCH_NEW_BYTE()                                                                                    \
if (inBitMask == 0x80)                                                                                             \
{                                                                                                                  \
    currByte = *inCursor;                                                                                          \
    if (inCursor - in >= size)                                                                                     \
    {                                                                                                              \
        currByte = 0;                                                                                              \
    }                                                                                                              \
    else                                                                                                           \
    {                                                                                                              \
        inCursor++;                                                                                                \
    }                                                                                                              \
    checksum += currByte;                                                                                          \
}

#define DEC_READ_FLAG_BIT()                                                                                            \
DEC_HANDLE_FETCH_NEW_BYTE();                                                                                       \
inBits = currByte & inBitMask;                                                                                     \
DEC_NEXT_BIT();

#define DEC_READ_BITS(bitsCount)                                                                                       \
outBitMask = 0x01 << (bitsCount - 1);                                                                              \
inBits = 0;                                                                                                        \
while (outBitMask != 0)                                                                                            \
{                                                                                                                  \
    DEC_HANDLE_FETCH_NEW_BYTE();                                                                                   \
    if ((currByte & inBitMask) != 0)                                                                               \
    {                                                                                                              \
        inBits |= outBitMask;                                                                                      \
    }                                                                                                              \
                                                                                                                    \
    outBitMask >>= 1;                                                                                              \
    DEC_NEXT_BIT();                                                                                                \
}

LPBYTE Lzss::decompress(LPBYTE in, int compressedSize, LPBYTE out, int decompressedSize)
{
    int i;
    uint32_t matchOffset;
    uint32_t inBits;
    int matchLength;
    uint32_t dictValue;
    uint32_t outBitMask;

    uint8_t inBitMask = 0x80;
    uint32_t currByte = 0;
    uint32_t checksum = 0;
    int size = compressedSize;

    if (out == NULL)
    {
        out = (uint8_t*)GlobalAlloc(GMEM_FIXED, decompressedSize);
        if (out == NULL)
        {
            return NULL;
        }
    }

    LPBYTE inCursor = in;
    LPBYTE outCursor = out;
    uint32_t dictHead = 1;

    for (;;)
    {
        DEC_READ_FLAG_BIT();

        // Read literal byte from next 8 bits
        if (inBits != 0)
        {
            DEC_READ_BITS(8);
            DEC_WRITE_BYTE(inBits);
        }
        // Copy from dictionary, 13 bit offset, then 4 bit length
        else
        {
            DEC_READ_BITS(13);

            matchOffset = inBits;
            if (matchOffset == 0)
            {
                break;
            }

            DEC_READ_BITS(4);

            // Value encoded in 4 bit length is 3 less than the actual length
            matchLength = inBits + 2;
            for (i = 0; i <= matchLength; i++)
            {
                dictValue = m_Dict[LZSS_DICTPOS_MOD(matchOffset, i)];
                DEC_WRITE_BYTE(dictValue);
            }
        }
    }

    // Read any trailing bits in the data
    while (inBitMask != 0x80)
    {
        DEC_READ_FLAG_BIT();
    }

    return out;
}

void Lzss::InitTree(int root)
{
    m_Tree[LZSS_DICTSIZE].right = root;
    m_Tree[root].parent = LZSS_DICTSIZE;
    m_Tree[root].right = 0;
    m_Tree[root].left = 0;
}

void Lzss::InitEncoderState()
{
    int i;

    for (i = 0; i < LZSS_DICTSIZE; i++)
    {
        m_Dict[i] = 0;
    }
    for (i = 0; i < LZSS_DICTSIZE + 1; i++)
    {
        m_Tree[i].parent = 0;
        m_Tree[i].left = 0;
        m_Tree[i].right = 0;
    }
}

int Lzss::AddString(int newNode, int* matchPosition)
{
    int i;
    int* child;
    int delta;

    if (newNode == 0)
    {
        return 0;
    }

    int testNode = m_Tree[LZSS_DICTSIZE].right;
    int matchLength = 0;

    for (;;)
    {
        for (i = 0; i < LZSS_LOOKAHEAD_SIZE; i++)
        {
            delta = m_Dict[LZSS_DICTPOS_MOD(newNode, i)] - m_Dict[LZSS_DICTPOS_MOD(testNode, i)];

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
                ReplaceNode(testNode, newNode);
                return matchLength;
            }
        }

        if (delta >= 0)
        {
            child = &m_Tree[testNode].right;
        }
        else
        {
            child = &m_Tree[testNode].left;
        }

        if (*child == 0)
        {
            *child = newNode;
            m_Tree[newNode].parent = testNode;
            m_Tree[newNode].right = 0;
            m_Tree[newNode].left = 0;
            return matchLength;
        }

        testNode = *child;
    }
}

void Lzss::DeleteString(int p)
{
    if (m_Tree[p].parent == 0)
    {
        return;
    }

    if (m_Tree[p].right == 0)
    {
        ContractNode(p, m_Tree[p].left);
    }
    else if (m_Tree[p].left == 0)
    {
        ContractNode(p, m_Tree[p].right);
    }
    else
    {
        int replacement = FindNextNode(p);
        DeleteString(replacement);
        ReplaceNode(p, replacement);
    }
}

void Lzss::ContractNode(int oldNode, int newNode)
{
    m_Tree[newNode].parent = m_Tree[oldNode].parent;

    if (m_Tree[m_Tree[oldNode].parent].right == oldNode)
    {
        m_Tree[m_Tree[oldNode].parent].right = newNode;
    }
    else
    {
        m_Tree[m_Tree[oldNode].parent].left = newNode;
    }
    m_Tree[oldNode].parent = 0;
}

void Lzss::ReplaceNode(int oldNode, int newNode)
{
    int parent = m_Tree[oldNode].parent;

    if (m_Tree[parent].left == oldNode)
    {
        m_Tree[parent].left = newNode;
    }
    else
    {
        m_Tree[parent].right = newNode;
    }
    m_Tree[newNode] = m_Tree[oldNode];
    m_Tree[m_Tree[newNode].left].parent = newNode;
    m_Tree[m_Tree[newNode].right].parent = newNode;
    m_Tree[oldNode].parent = 0;
}

int Lzss::FindNextNode(int node)
{
    int next = m_Tree[node].left;

    while (m_Tree[next].right != 0)
    {
        next = m_Tree[next].right;
    }
    return next;
}