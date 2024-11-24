#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <queue>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>

#define MAX_TREE_HT 100

using namespace std;

struct MinHeapNode {
    char character;
    int freq;
    MinHeapNode *left, *right;

    MinHeapNode(char c, int f) : character(c), freq(f), left(nullptr), right(nullptr) {}
};

struct Compare {
    bool operator()(MinHeapNode* l, MinHeapNode* r) {
        return l->freq > r->freq;
    }
};

class HuffmanCoding {
private:
    MinHeapNode* root;
    vector<int> freqArray;
    vector<string> codes;

    void buildHuffmanTree(const vector<int>& freq);
    void generateCodes(MinHeapNode* node, string code);
    void writeCodesToFile(ofstream &outFile);
    void compressFile(int fdIn, int fdOut);
    void decompressFile(int fdIn, int fdOut);

public:
    HuffmanCoding() : root(nullptr), freqArray(128, 0), codes(128) {}
    void compress(const string& inputFile, const string& outputFile);
    void decompress(const string& inputFile, const string& outputFile);
};

void HuffmanCoding::buildHuffmanTree(const vector<int>& freq) {
    priority_queue<MinHeapNode*, vector<MinHeapNode*>, Compare> minHeap;

    for (int i = 0; i < 128; i++) {
        if (freq[i] != 0)
            minHeap.push(new MinHeapNode((char)i, freq[i]));
    }

    while (minHeap.size() != 1) {
        MinHeapNode *left = minHeap.top();
        minHeap.pop();
        MinHeapNode *right = minHeap.top();
        minHeap.pop();

        MinHeapNode* top = new MinHeapNode('$', left->freq + right->freq);
        top->left = left;
        top->right = right;
        minHeap.push(top);
    }

    root = minHeap.top();
    generateCodes(root, "");
}

void HuffmanCoding::generateCodes(MinHeapNode* node, string code) {
    if (!node) return;

    if (!node->left && !node->right) {
        codes[node->character] = code;
    }

    generateCodes(node->left, code + "0");
    generateCodes(node->right, code + "1");
}

void HuffmanCoding::writeCodesToFile(ofstream &outFile) {
    int numUniqueChars = 0;
    for (int i = 0; i < 128; i++) {
        if (freqArray[i] > 0) numUniqueChars++;
    }

    outFile.write((char*)&numUniqueChars, sizeof(int));

    for (int i = 0; i < 128; i++) {
        if (freqArray[i] > 0) {
            outFile.put((char)i);
            int len = codes[i].length();
            outFile.write((char*)&len, sizeof(int));
            int binaryCode = stoi(codes[i], nullptr, 2);
            outFile.write((char*)&binaryCode, sizeof(int));
        }
    }
}

void HuffmanCoding::compressFile(int fdIn, int fdOut) {
    char c;
    unsigned char byte = 0;
    int bitPos = 0;

    while (read(fdIn, &c, sizeof(char)) != 0) {
        string code = codes[c];
        for (char bit : code) {
            if (bit == '1') byte |= (1 << (7 - bitPos));
            if (++bitPos == 8) {
                write(fdOut, &byte, sizeof(char));
                byte = 0;
                bitPos = 0;
            }
        }
    }

    if (bitPos > 0) {
        byte <<= (8 - bitPos);
        write(fdOut, &byte, sizeof(char));
    }
}

void HuffmanCoding::compress(const string& inputFile, const string& outputFile) {
    ifstream inFile(inputFile, ios::in);
    ofstream outFile(outputFile, ios::out | ios::binary);
    if (!inFile || !outFile) {
        cerr << "Error opening files!" << endl;
        return;
    }

    char ch;
    while (inFile.get(ch)) {
        freqArray[(unsigned char)ch]++;
    }
    inFile.clear();
    inFile.seekg(0);

    buildHuffmanTree(freqArray);
    writeCodesToFile(outFile);

    int fdIn = open(inputFile.c_str(), O_RDONLY);
    int fdOut = open(outputFile.c_str(), O_WRONLY | O_APPEND);
    compressFile(fdIn, fdOut);

    close(fdIn);
    close(fdOut);
    inFile.close();
    outFile.close();
}

void HuffmanCoding::decompressFile(int fdIn, int fdOut) {
    MinHeapNode* currentNode = root;
    char byte;
    while (read(fdIn, &byte, sizeof(char)) > 0) {
        for (int i = 0; i < 8; i++) {
            if ((byte & (1 << (7 - i))) != 0)
                currentNode = currentNode->right;
            else
                currentNode = currentNode->left;

            if (!currentNode->left && !currentNode->right) {
                write(fdOut, &currentNode->character, sizeof(char));
                currentNode = root;
            }
        }
    }
}

void HuffmanCoding::decompress(const string& inputFile, const string& outputFile) {
    ifstream inFile(inputFile, ios::in | ios::binary);
    ofstream outFile(outputFile, ios::out);
    if (!inFile || !outFile) {
        cerr << "Error opening files!" << endl;
        return;
    }

    int numUniqueChars, totalChars;
    inFile.read((char*)&numUniqueChars, sizeof(int));
    inFile.read((char*)&totalChars, sizeof(int));

    for (int i = 0; i < numUniqueChars; i++) {
        char character;
        int length, binaryCode;
        inFile.get(character);
        inFile.read((char*)&length, sizeof(int));
        inFile.read((char*)&binaryCode, sizeof(int));

        codes[character] = bitset<32>(binaryCode).to_string().substr(32 - length);
    }

    int fdIn = open(inputFile.c_str(), O_RDONLY);
    int fdOut = open(outputFile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    decompressFile(fdIn, fdOut);

    close(fdIn);
    close(fdOut);
    inFile.close();
    outFile.close();
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <c/d> <input_file> <output_file>" << endl;
        return 1;
    }

    HuffmanCoding huffman;
    if (strcmp(argv[1], "c") == 0) {
        huffman.compress(argv[2], argv[3]);
    } else if (strcmp(argv[1], "d") == 0) {
        huffman.decompress(argv[2], argv[3]);
    } else {
        cerr << "Invalid option. Use 'c' for compress and 'd' for decompress." << endl;
        return 1;
    }

    return 0;
}
