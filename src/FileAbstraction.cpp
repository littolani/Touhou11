#include "FileAbstrction.h"
#include "Globals.h"

int g_numEntriesInDatFile = 0;
PbgArchive g_pbgArchives[20];

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
    // Need a new byte
    if (m_bitBuffer == 0x80)
    {
        if (m_ptr < m_data + m_size)
            m_currentByte = *m_ptr++;
        else
            m_currentByte = 0; // Beyond data, pad with zeros
    }
    int bit = (m_currentByte & m_bitBuffer) != 0;
    m_bitBuffer >>= 1;
    if (m_bitBuffer == 0)
        m_bitBuffer = 0x80; // Reset to MSB after 8 shifts

    return bit;
}

int BitReader::readBits(int num_bits)
{
    int value = 0;
    for (int i = 0; i < num_bits; i++) {
        value = (value << 1) | readBit(); // Build value MSB to LSB
    }
    return value;
}

// 0x4426c0
byte* FileUtils::lzssDecompress(byte* compressedData, int compressedSize, byte* outBuffer, size_t decompressedSize)
{
    byte lzssDict[LZSS_DICTSIZE] = {0};

    if (!outBuffer)
    {
        outBuffer = new byte[decompressedSize];
        if (!outBuffer)
            return NULL;
    }

    byte* writePtr = outBuffer;
    uint32_t ringBufferIndex = 1;
    BitReader reader(compressedData, compressedSize);

    while (true)
    {
        int flag = reader.readBit();
        if (flag)
        {
            uint32_t byte_value = reader.readBits(8);
            *writePtr++ = (byte)byte_value;
            lzssDict[ringBufferIndex] = (byte)byte_value;
            ringBufferIndex = (ringBufferIndex + 1) & 0x1fff;
        }
        else
        {
            uint32_t match_offset = reader.readBits(13);
            if (match_offset == 0) {
                break;
            }
            uint32_t length_bits = reader.readBits(4);
            int length = length_bits + 3;
            for (int i = 0; i < length; i++) {
                byte b = lzssDict[(match_offset + i) & 0x1fff];
                *writePtr++ = b;
                lzssDict[ringBufferIndex] = b;
                ringBufferIndex = (ringBufferIndex + 1) & 0x1fff;
            }
        }
    }

    byte bitBuffer = reader.getBitBuffer();
    while (bitBuffer != 0x80 && bitBuffer != 0)
        reader.shiftBitBuffer();

    return outBuffer;
}

//0x4428d0
void FileUtils::resetEncoderState(byte* lzssDict, LzssTree* lzssTree)
{
    memset(lzssDict, 0, LZSS_DICTSIZE);
    memset(lzssTree, 0, sizeof(LzssTree) * LZSS_DICTSIZE);
}

//0x442b50
void FileUtils::lzssTreeReplaceNode(LzssTree* lzssTree, uint32_t oldNode, int newNode)
{
    uint32_t parent = lzssTree[oldNode].parent;

    if (lzssTree[parent].left == oldNode)
        lzssTree[parent].left = newNode;
    else
        lzssTree[parent].right = newNode;

    lzssTree[newNode].parent = lzssTree[oldNode].parent;
    lzssTree[newNode].left = lzssTree[oldNode].left;
    lzssTree[newNode].right = lzssTree[oldNode].right;
    lzssTree[lzssTree[newNode].left].parent = newNode;
    lzssTree[lzssTree[newNode].right].parent = newNode;
    lzssTree[oldNode].parent = 0;
}

// 0x442910
int FileUtils::lzssDictAddString(byte* lzssDict, LzssTree* lzssTree, uint32_t lzssTreeRoot, int* matchPosition, int newNode)
{
    if (newNode == 0)
        return 0;

    int matchLength = 0;
    int testNode = lzssTreeRoot;
    while (true)
    {
        int i;
        int delta = 0;
        for (i = 0; i < LZSS_MAX_MATCH; i++) {
            int idx1 = (newNode + i) & LZSS_DICTSIZE_MASK;
            int idx2 = (testNode + i) & LZSS_DICTSIZE_MASK;
            delta = (uint8_t)lzssDict[idx1] - (uint8_t)lzssDict[idx2];
            if (delta != 0) {
                break;
            }
        }
        if (i > matchLength) {
            *matchPosition = testNode;
            matchLength = i;
            if (i > LZSS_MAX_MATCH - 1) {
                lzssTreeReplaceNode(lzssTree, testNode, newNode);
                return i;
            }
        }
        int* child;
        if (delta < 0) {
            child = &lzssTree[testNode].left;
        } else {
            child = &lzssTree[testNode].right;
        }
        if (*child == 0) {
            *child = newNode;
            lzssTree[newNode].parent = testNode;
            lzssTree[newNode].left = 0;
            lzssTree[newNode].right = 0;
            return matchLength;
        } else {
            testNode = *child;
        }
    }
}

void FileUtils::encrypt(
    uint8_t* data,
    uint32_t size,
    uint8_t key,
    const uint8_t step,
    uint32_t block,
    uint32_t limit)
{
    const uint8_t* end;
    uint8_t* temp = new uint8_t[block];
    uint32_t increment = (block >> 1) + (block & 1);

    if (size < block >> 2)
        size = 0;
    else
        size -= (size % block < block >> 2) * size % block + size % 2;

    if (limit % block != 0)
        limit = limit + (block - (limit % block));

    end = data + (size < limit ? size : limit);

    while (data < end) {
        uint8_t* in;
        uint8_t* out = temp;
        if (end - data < (ptrdiff_t)block) {
            block = end - data;
            increment = (block >> 1) + (block & 1);
        }

        for (in = data + block - 1; in > data;) {
            *out = *in-- ^ key;
            *(out + increment) = *in-- ^ (key + step * increment);
            ++out;
            key += step;
        }

        if (block & 1) {
            *out = *in ^ key;
            key += step;
        }
        key += step * increment;

        memcpy(data, temp, block);
        data += block;
    }

    free(temp);
}

// 0x4581c0
void FileUtils::decrypt(uint8_t* data, uint32_t size, uint8_t key, const uint8_t step, uint32_t block, uint32_t limit)
{
    const uint8_t* end;
    uint8_t* temp = new uint8_t[block];
    uint32_t increment = (block >> 1) + (block & 1);

    if (size < block >> 2)
        size = 0;
    else
        size -= (size % block < block >> 2) * size % block + size % 2;

    if (limit % block != 0)
        limit = limit + (block - (limit % block));

    end = data + (size < limit ? size : limit);

    while (data < end) {
        uint8_t* in = data;
        uint8_t* out;
        if (end - data < (ptrdiff_t)block) {
            block = end - data;
            increment = (block >> 1) + (block & 1);
        }

        for (out = temp + block - 1; out > temp;) {
            *out-- = *in ^ key;
            *out-- = *(in + increment) ^ (key + step * increment);
            ++in;
            key += step;
        }

        if (block & 1) {
            *out = *in ^ key;
            key += step;
        }
        key += step * increment;

        memcpy(data, temp, block);
        data += block;
    }

    delete[] temp;
}

// 0x441bd0
LPSTR PbgArchive::copyFilename(LPCSTR filename)
{
    size_t size = strlen(filename) + 1;
    LPSTR buf = new char[size];
    if (buf)
        strcpy_s(buf, size, filename);
    return buf;
}

// 0x441d50
DWORD CPbgFile::open(const char* filename, const char* mode)
{
    DWORD creationDisposition;
    BOOL goToEnd = FALSE;
    char filePathBuffer[MAX_PATH];

    close();

    const char* curMode;
    for (curMode = mode; *curMode != '\0'; curMode++)
    {
        if (*curMode == 'r')
        {
            m_access = GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
            break;
        }
        if (*curMode == 'w')
        {
            DeleteFileA(filename);
            m_access = GENERIC_WRITE;
            creationDisposition = CREATE_ALWAYS;
            break;
        }
        if (*curMode == 'a')
        {
            goToEnd = TRUE;
            m_access = GENERIC_WRITE;
            creationDisposition = OPEN_ALWAYS;
            break;
        }
    }

    if (*curMode == '\0')
        return false;

    getFullFilePath(filePathBuffer, filename);
    m_handle = CreateFileA(
        filePathBuffer,
        m_access,
        FILE_SHARE_READ,
        NULL,
        creationDisposition,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (m_handle == INVALID_HANDLE_VALUE)
        return false;

    if (goToEnd)
    {
        SetFilePointer(
            m_handle,
            0,
            NULL,
            FILE_END
        );
    }
    return true;
}

DWORD CPbgFile::read(LPVOID data, DWORD dataLen)
{
    DWORD numBytesRead = 0;

    if (m_access != GENERIC_READ)
        return 0;

    ReadFile(m_handle, data, dataLen, &numBytesRead, NULL);
    return numBytesRead;
}

bool CPbgFile::write(LPVOID data, DWORD dataLen)
{
    DWORD outWritten = 0;

    if (m_access != GENERIC_WRITE)
        return false;

    WriteFile(m_handle, data, dataLen, &outWritten, NULL);
    return dataLen == outWritten ? true : false;
}

DWORD CPbgFile::tell()
{
    if (m_handle == INVALID_HANDLE_VALUE)
        return 0;

    return SetFilePointer(m_handle, 0, NULL, FILE_CURRENT);
}

DWORD CPbgFile::getSize()
{
    if (m_handle == INVALID_HANDLE_VALUE)
        return 0;

    return GetFileSize(m_handle, NULL);
}

bool CPbgFile::seek(DWORD offset, DWORD seekFrom)
{
    if (m_handle == INVALID_HANDLE_VALUE)
        return false;

    SetFilePointer(
        m_handle,
        offset,
        NULL,
        seekFrom
    );
    return true;
}

void CPbgFile::close()
{
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
        m_access = 0;
    }
}

HGLOBAL CPbgFile::readWholeFile(DWORD maxSize)
{  
    if (m_access != GENERIC_READ)
        return nullptr;

    DWORD size = getSize();
    if (maxSize < size)
        return nullptr;
  
    void* data = malloc(size);
    if (!data)
        return nullptr;

    DWORD offset = tell();

    if (!seek(offset, 0))
        return nullptr;

    if (!read(data, size))
    {
        free(data);
        return nullptr;
    }
    
    seek(offset, 0);
    return data;
}

void PbgArchive::release()
{
    if (m_filename)
        printf("info : %s close arcfile\n", m_filename);

    delete[] m_filename;
    delete[] m_entries;
    delete m_fileAbstraction;
    m_numEntries = 0;
}

// 0x4418d0
PbgArchiveEntry* PbgArchive::findEntry(LPCSTR filename)
{
    int filesMatch;
    int length;
    PbgArchiveEntry* entries;
    
    entries = m_entries;
    if (!entries)
        return nullptr;

    length = m_numEntries;
    while (true)
    {
        if (length < 1) {
            return nullptr;
        }
        filesMatch = _stricmp(filename, m_filename);
        if (filesMatch == 0) 
            break;
        --length;
        ++entries;
    }
    return entries;
}

// 0x4415c0
bool PbgArchive::load(LPCSTR filename)
{
    bool loadFileResult;
    CPbgFile *file;
    LPSTR name;
    int status;
    char *outFileName;
  
    release();
    printf("info : %s open arcfile\n", filename);
    m_fileAbstraction = new CPbgFile();
    if (!m_fileAbstraction)
        return false;

    if (parseHeader(filename))
    { 
        m_filename = copyFilename(filename);
        if (m_filename)
        {
            m_fileAbstraction->open(m_filename, "r");
            return true;
        }
    }
    printf("info : %s not found\n", filename);
    release();
    return false;
}

// 0x441910
bool PbgArchive::parseHeader(LPCSTR filename)
{
    byte* decompressedFile;

    union {
        PbgArchiveHeader asHeader;
        byte asBytes[sizeof(PbgArchiveHeader)];
    } header;
  
    if (!m_fileAbstraction)
      return false;
    if (!m_fileAbstraction->open(filename, "r"))
        goto ParseError;
    if (!m_fileAbstraction->read(header.asBytes, sizeof(PbgArchiveHeader)))
        goto ParseError;
            
    FileUtils::decrypt(header.asBytes, 0x10, 0x1b, 0x37,0x10,0x10);

    if (header.asHeader.magicNumber != 0x31414854) // "THA1" in little endian
        goto ParseError;

    // ZUN's amazing magic numbers
    int decompressedSize = header.asHeader.decompressedSize - 123456789;
    int fileTableOffset = header.asHeader.fileTableOffset - 987654321;
    int numEntries = header.asHeader.numEntries + 135792468;
    m_numEntries = numEntries;

    DWORD seekOffset = m_fileAbstraction->getSize() - fileTableOffset;
    m_fileAbstraction->seek(seekOffset, 0);
    uint32_t length = header.asHeader.fileTableOffset;
    byte* archive = new byte[header.asHeader.fileTableOffset];
    if (!archive)
        goto ParseError;

    if (m_fileAbstraction->read(archive, length) == 0) 
    {
        decompressedFile = nullptr;
        goto ParseError;
    }

    FileUtils::decrypt(archive, length, 0x3e, 0x9b, 0x80, length);

    decompressedFile = FileUtils::lzssDecompress(archive,
        length,
        nullptr,
        decompressedSize
    );
    
    if (decompressedFile)
    {
        PbgArchiveEntry* entries = loadEntries(decompressedFile, m_numEntries,seekOffset);
        m_entries = entries;

        // All good
        if (entries)
        {
            delete[] archive;
            delete[] decompressedFile;
            return true;
        }
    }

    delete[] archive;
    if (decompressedFile)
        delete[] decompressedFile;

ParseError:
  if (m_fileAbstraction)
    m_fileAbstraction->~CPbgFile();

  m_fileAbstraction = nullptr;
  return false;
}

// 0x441a60
PbgArchiveEntry* PbgArchive::loadEntries(byte* bytes, int numEntries, uint32_t seekOffset)
{
    uint32_t entriesToAllocate = numEntries + 1;
    uint64_t sizeInBytes = static_cast<uint64_t>(entriesToAllocate) * sizeof(PbgArchiveEntry);

    uint32_t bufferSize;
    if (sizeInBytes > 0xfffffffb)
    {
        printf("PbgArchive::loadEntries: Max allocation size of 4GB reached!\n");
        bufferSize = 0xfffffffb;
    }
    else
        bufferSize = sizeInBytes;

    uint32_t* buffer = new uint32_t[bufferSize / 4 + 1]; // Adjusted for uint32_t array
    if (!buffer)
    {
        printf("PbgArchive::loadEntries: PbgArchive allocation failed!\n");
        return nullptr;
    }

    // Store size in first 4 bytes
    *buffer = entriesToAllocate;
    PbgArchiveEntry* entries = reinterpret_cast<PbgArchiveEntry*>(buffer + 1);
    memset(entries, 0, entriesToAllocate * sizeof(PbgArchiveEntry));

    byte* bufferPtr = bytes;
    PbgArchiveEntry* entryPtr = entries;
    int remainingEntries = numEntries;

    // Read the entries into memory
    while (remainingEntries > 0)
    {
        byte* start = bufferPtr;
        while (*bufferPtr != 0)
            bufferPtr++;
        size_t filenameLength = bufferPtr - start + 1;

        char* filename = new char[filenameLength];
        if (!filename)
        {
            printf("PbgArchive::loadEntries: filename allocation failed!\n");
            return nullptr;
        }

        memcpy(filename, start, filenameLength);
        entryPtr->filename = filename;

        bufferPtr++;
        size_t offset = bufferPtr - start;
        int padding = (4 - (offset % 4)) % 4;
        bufferPtr += padding;

        memcpy(&entryPtr->dataOffset, bufferPtr, sizeof(uint32_t));
        bufferPtr += 4;
        memcpy(&entryPtr->decompressedSize, bufferPtr, sizeof(uint32_t));
        bufferPtr += 4;
        memcpy(&entryPtr->unk, bufferPtr, sizeof(uint32_t));
        bufferPtr += 4;

        entryPtr++;
        remainingEntries--;
    }
    entryPtr->dataOffset = seekOffset;
    entryPtr->decompressedSize = 0;
    return entries;
}

// 0x441760
byte* PbgArchive::loadWithDecryptionKeyset(byte* destBuffer, LPCSTR requestedFilename)
{
    byte* fileBuffer = nullptr;

    // Check if file abstraction exists
    if (!m_fileAbstraction)
        return nullptr;

    // Find the archive entry for the requested file
    PbgArchiveEntry* fileEntry = findEntry(requestedFilename);
    if (!fileEntry)
        goto LoadError;

    // Calculate sizes (sentinel entry ensures bounds safety)
    size_t compressedSize = fileEntry[1].dataOffset - fileEntry->dataOffset;
    size_t decompressedSize = fileEntry->decompressedSize;

    // Use destBuffer if no decompression is needed and it’s provided; otherwise, allocate
    if (compressedSize == decompressedSize && destBuffer != nullptr)
        fileBuffer = destBuffer;
    else
        fileBuffer = new byte[compressedSize];

    if (!fileBuffer)
        goto LoadError;

    // Seek to the data position in the archive
    if (!m_fileAbstraction->seek(fileEntry->dataOffset, 0))
        goto LoadError;

    // Read the compressed data
    if (!m_fileAbstraction->read(fileBuffer, compressedSize))
        goto LoadError;

    // Calculate filename checksum for decryption key selection
    uint8_t sum = 0;
    for (const char* p = fileEntry->filename; *p != '\0'; ++p) {
        sum += static_cast<uint8_t>(*p);
    }
    uint32_t keyIndex = sum & 7; // Selects key index (0-7)

    // Decrypt the data in place
    FileUtils::decrypt(
        fileBuffer,
        compressedSize,
        FileUtils::s_decryptionKeySet[keyIndex].key,
        FileUtils::s_decryptionKeySet[keyIndex].step,
        FileUtils::s_decryptionKeySet[keyIndex].block,
        FileUtils::s_decryptionKeySet[keyIndex].limit
    );

    // Handle decompression if necessary
    byte* decompressedFile = fileBuffer;
    if (compressedSize != decompressedSize) {
        decompressedFile = FileUtils::lzssDecompress(
            fileBuffer,
            compressedSize,
            destBuffer,
            decompressedSize
        );
    }

    // Free fileBuffer if it was allocated (not destBuffer)
    if (fileBuffer != destBuffer && fileBuffer)
        delete[] fileBuffer;

    return decompressedFile;

LoadError:
    printf("PbgArchive: error loading archive\n");
    if (fileBuffer)
        delete[] fileBuffer;
    return nullptr;
}

void PbgArchive::loadTh11Dat()
{
    size_t outSize;
    char buffer [128];

    if (!load(&g_pbgArchive, "th11.dat"))
    {
        printf("error : データファイルが存在しません\n");
        return;
    }
    sprintf_s(buffer, "th11_%.4x%c.ver", 0x100, 0x61);
    g_supervisor.th11DatBytes = openFile(buffer, &outSize, 0);
    g_supervisor.th11DatSize = outSize;
    if (!g_supervisor.th11DatBytes)
    {
        printf("error : データのバージョンが違います\n");
        g_supervisor.th11DatBytes = nullptr;
    }
}