
#ifndef ZF_H
#define ZF_H

#include <stddef.h>   // for size_t

#ifdef __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_SYMBOLS 256
// This constant can be avoided by explicitly
// calculating height of Huffman Tree
#define MAX_TREE_HT 100

// A Huffman tree node
struct MinHeapNode {

    // One of the input characters
    char data;

    // Frequency of the character
    unsigned freq;

    // Left and right child of this node
    struct MinHeapNode *left, *right;
};

// A Min Heap: Collection of
// min-heap (or Huffman tree) nodes
struct MinHeap {

    // Current size of min heap
    unsigned size;

    // capacity of min heap
    unsigned capacity;

    // Array of minheap node pointers
    struct MinHeapNode** array;
};

// A utility function allocate a new
// min heap node with given character
// and frequency of the character
struct MinHeapNode* newNode(char data, unsigned freq)
{
    struct MinHeapNode* temp = (struct MinHeapNode*)malloc(
        sizeof(struct MinHeapNode));

    temp->left = temp->right = NULL;
    temp->data = data;
    temp->freq = freq;

    return temp;
}

// A utility function to create
// a min heap of given capacity
struct MinHeap* createMinHeap(unsigned capacity)

{

    struct MinHeap* minHeap
        = (struct MinHeap*)malloc(sizeof(struct MinHeap));

    // current size is 0
    minHeap->size = 0;

    minHeap->capacity = capacity;

    minHeap->array = (struct MinHeapNode**)malloc(
        minHeap->capacity * sizeof(struct MinHeapNode*));
    return minHeap;
}

// A utility function to
// swap two min heap nodes
void swapMinHeapNode(struct MinHeapNode** a,
                     struct MinHeapNode** b)

{

    struct MinHeapNode* t = *a;
    *a = *b;
    *b = t;
}

// The standard minHeapify function.
void minHeapify(struct MinHeap* minHeap, int idx)

{

    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < minHeap->size
        && minHeap->array[left]->freq
               < minHeap->array[smallest]->freq)
        smallest = left;

    if (right < minHeap->size
        && minHeap->array[right]->freq
               < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        swapMinHeapNode(&minHeap->array[smallest],
                        &minHeap->array[idx]);
        minHeapify(minHeap, smallest);
    }
}

// A utility function to check
// if size of heap is 1 or not
int isSizeOne(struct MinHeap* minHeap)
{

    return (minHeap->size == 1);
}

// A standard function to extract
// minimum value node from heap
struct MinHeapNode* extractMin(struct MinHeap* minHeap)

{

    struct MinHeapNode* temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];

    --minHeap->size;
    minHeapify(minHeap, 0);

    return temp;
}

// A utility function to insert
// a new node to Min Heap
void insertMinHeap(struct MinHeap* minHeap,
                   struct MinHeapNode* minHeapNode)

{

    ++minHeap->size;
    int i = minHeap->size - 1;

    while (i
           && minHeapNode->freq
                  < minHeap->array[(i - 1) / 2]->freq) {

        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }

    minHeap->array[i] = minHeapNode;
}

// A standard function to build min heap
void buildMinHeap(struct MinHeap* minHeap)

{

    int n = minHeap->size - 1;
    int i;

    for (i = (n - 1) / 2; i >= 0; --i)
        minHeapify(minHeap, i);
}

// A utility function to print an array of size n
void printArr(int arr[], int n)
{
    int i;
    for (i = 0; i < n; ++i)
        printf("%d", arr[i]);

    printf("\n");
}

int isLeaf(struct MinHeapNode* root)

{

    return !(root->left) && !(root->right);
}
struct MinHeap* createAndBuildMinHeap(char data[],
                                      int freq[], int size)

{

    struct MinHeap* minHeap = createMinHeap(size);

    for (int i = 0; i < size; ++i)
        minHeap->array[i] = newNode(data[i], freq[i]);

    minHeap->size = size;
    buildMinHeap(minHeap);

    return minHeap;
}
struct MinHeapNode* buildHuffmanTree(char data[],
                                     int freq[], int size)

{
    struct MinHeapNode *left, *right, *top;
    struct MinHeap* minHeap
        = createAndBuildMinHeap(data, freq, size);
    while (!isSizeOne(minHeap)) {
        left = extractMin(minHeap);
        right = extractMin(minHeap);
        top = newNode('$', left->freq + right->freq);

        top->left = left;
        top->right = right;

        insertMinHeap(minHeap, top);
    }
    return extractMin(minHeap);
}
void printCodes(struct MinHeapNode* root, int arr[],
                int top)

{

    // Assign 0 to left edge and recur
    if (root->left) {

        arr[top] = 0;
        printCodes(root->left, arr, top + 1);
    }

    // Assign 1 to right edge and recur
    if (root->right) {

        arr[top] = 1;
        printCodes(root->right, arr, top + 1);
    }
    if (isLeaf(root)) {

        printf("%c: ", root->data);
        printArr(arr, top);
    }
}
void HuffmanCodes(char data[], int freq[], int size)

{
    // Construct Huffman Tree
    struct MinHeapNode* root
        = buildHuffmanTree(data, freq, size);

    // Print Huffman codes using
    // the Huffman tree built above
    int arr[MAX_TREE_HT], top = 0;

    printCodes(root, arr, top);
}

void writeBit(unsigned char* buf, int* bit_ptr, int bit) {
    if (bit) buf[*bit_ptr / 8] |= (1 << (7 - (*bit_ptr % 8)));
    (*bit_ptr)++;
}

int readBit(const unsigned char* buf, int* bit_ptr) {
    int bit = (buf[*bit_ptr / 8] >> (7 - (*bit_ptr % 8))) & 1;
    (*bit_ptr)++;
    return bit;
}

void serializeTree(struct MinHeapNode* root, unsigned char* buf, int* bit_ptr) {
    if (!root->left && !root->right) {
        writeBit(buf, bit_ptr, 1); // 1 = Лист
        for (int i = 7; i >= 0; i--) 
            writeBit(buf, bit_ptr, (root->data >> i) & 1);
        return;
    }
    writeBit(buf, bit_ptr, 0); // 0 = Узел
    serializeTree(root->left, buf, bit_ptr);
    serializeTree(root->right, buf, bit_ptr);
}

struct MinHeapNode* deserializeTree(const unsigned char* buf, int* bit_ptr) {
    if (readBit(buf, bit_ptr) == 1) {
        unsigned char data = 0;
        for (int i = 7; i >= 0; i--)
            if (readBit(buf, bit_ptr)) data |= (1 << i);
        return newNode(data, 0);
    }
    struct MinHeapNode* node = newNode(0, 0);
    node->left = deserializeTree(buf, bit_ptr);
    node->right = deserializeTree(buf, bit_ptr);
    return node;
}

void fillTable(struct MinHeapNode* root, char* str, int top, char* table[]) {
    if (root->left) { str[top] = '0'; fillTable(root->left, str, top + 1, table); }
    if (root->right) { str[top] = '1'; fillTable(root->right, str, top + 1, table); }
    if (!root->left && !root->right) {
        str[top] = '\0';
        table[(unsigned char)root->data] = strdup(str); // Фикс индекса для кириллицы
    }
}

unsigned char* zfit(unsigned char* in, size_t size, size_t* out_len) {
    int freqs[MAX_SYMBOLS] = {0};
    for (size_t i = 0; i < size; i++) freqs[in[i]]++;

    unsigned char unique_chars[MAX_SYMBOLS];
    int unique_freqs[MAX_SYMBOLS];
    int n = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (freqs[i] > 0) { 
            unique_chars[n] = (unsigned char)i; 
            unique_freqs[n] = freqs[i]; 
            n++; 
        }
    }

    struct MinHeapNode* root = buildHuffmanTree(unique_chars, unique_freqs, n);
    
    // Обязательно зануляем таблицу, чтобы free не упал
    char* table[MAX_SYMBOLS];
    for(int i = 0; i < MAX_SYMBOLS; i++) table[i] = NULL;
    
    char str_buf[MAX_SYMBOLS];
    fillTable(root, str_buf, 0, table);

    unsigned char* result = (unsigned char*)calloc(size + 2048, 1);
    memcpy(result, &size, sizeof(size_t));

    int bit_ptr = sizeof(size_t) * 8;
    serializeTree(root, result, &bit_ptr);

    for (size_t i = 0; i < size; i++) {
        char* code = table[in[i]]; 
        for (int j = 0; code[j]; j++) 
            writeBit(result, &bit_ptr, code[j] == '1');
    }

    *out_len = (bit_ptr + 7) / 8;
    for (int i = 0; i < MAX_SYMBOLS; i++) if (table[i]) free(table[i]);
    return result;
}

unsigned char* unzfit(unsigned char* in) {
    size_t original_size;
    memcpy(&original_size, in, sizeof(size_t));

    int bit_ptr = sizeof(size_t) * 8;
    struct MinHeapNode* root = deserializeTree(in, &bit_ptr);

    unsigned char* out = (unsigned char*)malloc(original_size + 1);
    struct MinHeapNode* curr = root;
    size_t decoded = 0;

    while (decoded < original_size) {
        int bit = readBit(in, &bit_ptr);
        curr = bit ? curr->right : curr->left;
        if (!curr->left && !curr->right) {
            out[decoded++] = (unsigned char)curr->data;
            curr = root;
        }
    }
    out[original_size] = '\0';
    return out;
}

#ifdef __cplusplus
}
#endif

#endif
