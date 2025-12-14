#pragma once

#define LZSS_OFFSET_BITS 13
#define LZSS_LENGTH_BITS 4
#define LZSS_DICTSIZE (1 << LZSS_OFFSET_BITS)

class Lzss
{
public:
    static LPBYTE compress(LPBYTE in, int uncompressedSize, int* compressedSize);
    static LPBYTE decompress(LPBYTE in, int compressedSize, LPBYTE out, int decompressedSize);

    static void InitTree(int root);
    static void InitEncoderState();
    static int AddString(int newNode, int* matchPosition);
    static void DeleteString(int p);
    static void ContractNode(int oldNode, int newNode);
    static void ReplaceNode(int oldNode, int newNode);
    static int FindNextNode(int node);

private:
    struct TreeNode
    {
        int parent;
        int left;
        int right;
    };

    static TreeNode m_Tree[LZSS_DICTSIZE + 1];
    static uint8_t m_Dict[LZSS_DICTSIZE];
};