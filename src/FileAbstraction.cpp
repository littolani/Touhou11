#include "FileAbstrction.h"

PbgArchive g_pbgArchive{};
int g_numEntriesInDatFile = 0;
PbgArchive g_pbgArchives[20];

#if 0
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
#endif

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
    delete[] temp;
}

/**
  * @brief Address: 0x4581c0
  * @brief Decrypts game files (Imported from ThAnm)
  * @param data:  Stack[0x4]:4
  * @param size:  Stack[0x8]:4
  * @param key:   AL:1
  * @param step:  Stack[0xc]:1
  * @param block: Stack[0x10]:4
  * @param limit: Stack[0x14]:4
  */
void __cdecl FileUtils::decrypt(uint8_t* data, uint32_t size, uint8_t key, uint8_t step, uint32_t block, uint32_t limit)
{
    //printf("Decrypt: Key=%d, Step=%d, Block=%d, Limit=%d, Size=%d, DataPtr=%p\n",
    //    key, step, block, limit, size, data);
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

    while (data < end)
    {
        uint8_t* in = data;
        uint8_t* out;
        if (end - data < (ptrdiff_t)block)
        {
            block = end - data;
            increment = (block >> 1) + (block & 1);
        }

        for (out = temp + block - 1; out > temp;)
        {
            *out-- = *in ^ key;
            *out-- = *(in + increment) ^ (key + step * increment);
            ++in;
            key += step;
        }

        if (block & 1)
        {
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
LPSTR PbgArchive::copyFileName(LPCSTR fileName)
{
    size_t size = strlen(fileName) + 1;
    LPSTR buf = new char[size];
    if (buf)
        strcpy_s(buf, size, fileName);
    return buf;
}

// 0x441cb0
CPbgFile::CPbgFile()
{
    m_handle = INVALID_HANDLE_VALUE;
    m_access = 0;
}

CPbgFile::~CPbgFile()
{
    close();
}

void CPbgFile::getFullFilePath(char* buffer, const char* filename)
{
    if (strchr(filename, ':') != NULL)
        strcpy_s(buffer, MAX_PATH, filename);
    else
    {
        GetModuleFileNameA(NULL, buffer, MAX_PATH);

        char* endOfModulePath = strrchr(buffer, '\\');
        if (endOfModulePath == NULL)
            *buffer = '\0';
        else
            endOfModulePath[1] = '\0';
        strcat_s(buffer, MAX_PATH, filename);
    }
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

PbgArchive::PbgArchive()
{
    m_entries = NULL;
    m_numEntries = 0;
    m_filename = NULL;
    m_fileAbstraction = NULL;
}

PbgArchive::~PbgArchive()
{
    release();
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
    release();
    printf("info : %s open arcfile\n", filename);
    m_fileAbstraction = new CPbgFile();
    if (!m_fileAbstraction)
        return false;

    if (parseHeader(filename))
    { 
        m_filename = copyFileName(filename);
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
    byte* archive;
    int decompressedSize = 0;
    int fileTableOffset = 0;
    int numEntries = 0;
    uint32_t length = 0;
    DWORD seekOffset = 0;

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
    decompressedSize = header.asHeader.decompressedSize - 123456789;
    fileTableOffset = header.asHeader.fileTableOffset - 987654321;
    numEntries = header.asHeader.numEntries + 135792468;
    m_numEntries = numEntries;

    seekOffset = m_fileAbstraction->getSize() - fileTableOffset;
    m_fileAbstraction->seek(seekOffset, 0);
    length = header.asHeader.fileTableOffset;
    archive = new byte[header.asHeader.fileTableOffset];
    if (!archive)
        goto ParseError;

    if (m_fileAbstraction->read(archive, length) == 0) 
    {
        decompressedFile = nullptr;
        goto ParseError;
    }

    FileUtils::decrypt(archive, length, 0x3e, 0x9b, 0x80, length);

    decompressedFile = Lzss::decompress(archive,
        length,
        nullptr,
        decompressedSize
    );
    
    if (decompressedFile)
    {
        PbgArchiveEntry* entries = allocEntries(decompressedFile, m_numEntries, seekOffset);
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
PbgArchiveEntry* PbgArchive::allocEntries(LPVOID entryBuffer, int count, uint32_t dataOffset)
{
    LPVOID entryData;
    int i;
    PbgArchiveEntry* buffer = nullptr;

    buffer = new PbgArchiveEntry[count + 1]();
    if (buffer == nullptr)
    {
        goto buffer_alloc_error;
    }

    entryData = entryBuffer;
    for (i = 0; i < count; i++)
    {
        buffer[i].filename = copyFileName((char*)entryData);
        seekPastString(&entryData);
        buffer[i].dataOffset = *(uint32_t*)entryData;
        seekPastInt(&entryData);
        buffer[i].decompressedSize = *(uint32_t*)entryData;
        seekPastInt(&entryData);
        buffer[i].unk = *(uint32_t*)entryData;
        seekPastInt(&entryData);
    }

    buffer[count].dataOffset = dataOffset;
    buffer[count].decompressedSize = 0;
    return buffer;

buffer_alloc_error:
    if (buffer)
    {
        delete[] buffer;
        buffer = nullptr;
    }
    return nullptr;
}

int PbgArchive::seekPastInt(LPVOID* ptr)
{
    *ptr = (int*)*ptr + 1;
    return *(int*)*ptr;
}

LPVOID PbgArchive::seekPastString(LPVOID* ptr)
{
    *ptr = (char*)*ptr + (strlen((char*)*ptr) + 1);
    return *ptr;
}

#if 0
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
        bufferSize = static_cast<uint32_t>(sizeInBytes);

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
#endif

// 0x441760
byte* PbgArchive::readDecompressEntry(LPCSTR filename, byte* outBuffer)
{
    byte* fileBuffer = nullptr;
    size_t compressedSize= 0;
    size_t decompressedSize = 0;
    uint8_t sum = 0;
    uint32_t keyIndex = 0;
    byte* decompressedFile = 0;

    // Check if file abstraction exists
    if (!m_fileAbstraction)
        return nullptr;

    // Find the archive entry for the requested file
    PbgArchiveEntry* fileEntry = findEntry(filename);
    if (!fileEntry)
        goto LoadError;

    // Calculate sizes (sentinel entry ensures bounds safety)
    compressedSize = fileEntry[1].dataOffset - fileEntry->dataOffset;
    decompressedSize = fileEntry->decompressedSize;

    // Use destBuffer if no decompression is needed and it’s provided; otherwise, allocate
    if (compressedSize == decompressedSize && outBuffer != nullptr)
        fileBuffer = outBuffer;
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
    for (const char* p = fileEntry->filename; *p != '\0'; ++p) {
        sum += static_cast<uint8_t>(*p);
    }
    keyIndex = sum & 7; // Selects key index (0-7)

    // Decrypt the data in place
    FileUtils::decrypt(
        fileBuffer,
        compressedSize,
        decryptionKeySet[keyIndex].key,
        decryptionKeySet[keyIndex].step,
        decryptionKeySet[keyIndex].block,
        decryptionKeySet[keyIndex].limit
    );

    // Handle decompression if necessary
    decompressedFile = fileBuffer;
    if (compressedSize != decompressedSize) {
        decompressedFile = Lzss::decompress(
            fileBuffer,
            compressedSize,
            outBuffer,
            decompressedSize
        );
    }

    // Free fileBuffer if it was allocated (not destBuffer)
    if (fileBuffer != outBuffer && fileBuffer)
        delete[] fileBuffer;

    return decompressedFile;

LoadError:
    printf("PbgArchive: error loading archive\n");
    if (fileBuffer)
        delete[] fileBuffer;
    return nullptr;
}

DWORD PbgArchive::getEntryDecompressedSize(LPCSTR filename)
{
    PbgArchiveEntry* entry = findEntry(filename);
    if (entry != NULL)
        return entry->decompressedSize;
    return 0;
}

// 0x4420e0
PbgArchive* findMatchingArchive(const char* filename)
{
    char filenameBuffer[260];
    // Copy the input filename to buffer safely
    strcpy_s(filenameBuffer, sizeof(filenameBuffer), filename);

    // Find last directory separator
    const char* lastSlashPtr = strrchr(filename, '/');
    if (lastSlashPtr)
    {
        // Calculate position after last '/'
        size_t slashPos = lastSlashPtr - filename;
        strcpy_s(filenameBuffer + slashPos, sizeof(filenameBuffer) - slashPos, ".dat");
    }

    // Search for matching archive
    for (int i = 0; i < g_numEntriesInDatFile; i++)
    {
        if (strcmp(g_pbgArchives[i].m_filename, filenameBuffer) == 0)
            return &g_pbgArchives[i];
    }

    return nullptr;
}