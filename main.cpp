#include <iostream>
#include <fstream>
#include <queue>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <iomanip>

using namespace std;

struct Node {
    unsigned char ch;
    uint64_t freq;
    Node* left;
    Node* right;

    Node(unsigned char c, uint64_t f)
        : ch(c), freq(f), left(nullptr), right(nullptr) {}

    Node(Node* l, Node* r)
        : ch(0), freq(l->freq + r->freq), left(l), right(r) {}

    bool isLeaf() const {
        return left == nullptr && right == nullptr;
    }
};

struct Compare {
    bool operator()(Node* a, Node* b) const {
        return a->freq > b->freq;
    }
};

void deleteTree(Node* root) {
    if (!root) return;
    deleteTree(root->left);
    deleteTree(root->right);
    delete root;
}

Node* buildTree(const uint64_t freq[256]) {
    priority_queue<Node*, vector<Node*>, Compare> pq;

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            pq.push(new Node(static_cast<unsigned char>(i), freq[i]));
        }
    }

    if (pq.empty()) return nullptr;

    while (pq.size() > 1) {
        Node* left = pq.top();
        pq.pop();

        Node* right = pq.top();
        pq.pop();

        pq.push(new Node(left, right));
    }

    return pq.top();
}

void generateCodes(Node* root, const string& prefix,
                   unordered_map<unsigned char, string>& codes) {
    if (!root) return;

    if (root->isLeaf()) {
        codes[root->ch] = prefix.empty() ? "0" : prefix;
        return;
    }

    generateCodes(root->left, prefix + "0", codes);
    generateCodes(root->right, prefix + "1", codes);
}

bool readFile(const string& filename, vector<unsigned char>& data) {
    ifstream in(filename, ios::binary);

    if (!in) {
        cerr << "Error: Cannot open input file.\n";
        return false;
    }

    char c;
    while (in.get(c)) {
        data.push_back(static_cast<unsigned char>(c));
    }

    if (in.bad()) {
        cerr << "Error reading input file.\n";
        return false;
    }

    return true;
}

bool compressFile(const string& inputName,
                  const string& outputName) {
    if (inputName == outputName) {
        cerr << "Input and output filenames must differ.\n";
        return false;
    }

    vector<unsigned char> data;

    if (!readFile(inputName, data)) return false;

    uint64_t freq[256] = {};

    for (unsigned char c : data) {
        freq[c]++;
    }

    ofstream out(outputName, ios::binary);

    if (!out) {
        cerr << "Error: Cannot create compressed file.\n";
        return false;
    }

    // File format: 4-byte signature, original size,
    // 256 frequencies, then the compressed bit stream.
    out.write("HUF1", 4);

    uint64_t originalSize = static_cast<uint64_t>(data.size());
    out.write(reinterpret_cast<const char*>(&originalSize),
              sizeof(originalSize));

    for (int i = 0; i < 256; i++) {
        out.write(reinterpret_cast<const char*>(&freq[i]),
                  sizeof(freq[i]));
    }

    if (!out) {
        cerr << "Error writing compressed file header.\n";
        return false;
    }

    Node* root = buildTree(freq);
    unordered_map<unsigned char, string> codes;
    generateCodes(root, "", codes);

    unsigned char buffer = 0;
    int bitCount = 0;

    for (unsigned char c : data) {
        for (char bit : codes[c]) {
            buffer = static_cast<unsigned char>(
                (buffer << 1) | (bit == '1' ? 1 : 0)
            );

            bitCount++;

            if (bitCount == 8) {
                out.put(static_cast<char>(buffer));
                buffer = 0;
                bitCount = 0;
            }
        }
    }

    if (bitCount > 0) {
        buffer = static_cast<unsigned char>(buffer << (8 - bitCount));
        out.put(static_cast<char>(buffer));
    }

    bool success = static_cast<bool>(out);
    out.close();
    deleteTree(root);

    if (!success) {
        cerr << "Error writing compressed data.\n";
        return false;
    }

    cout << "\nCompression successful!\n";
    cout << "Original size   : " << originalSize << " bytes\n";

    ifstream compressed(outputName, ios::binary | ios::ate);
    uint64_t compressedSize =
        static_cast<uint64_t>(compressed.tellg());

    cout << "Compressed size : " << compressedSize << " bytes\n";

    if (originalSize > 0) {
        double reduction =
            (1.0 - static_cast<double>(compressedSize) /
                       static_cast<double>(originalSize)) * 100.0;

        cout << fixed << setprecision(2);
        cout << "Size reduction  : " << reduction << "%\n";
    } else {
        cout << "Size reduction  : N/A (empty file)\n";
    }

    return true;
}

bool decompressFile(const string& inputName,
                    const string& outputName) {
    if (inputName == outputName) {
        cerr << "Input and output filenames must differ.\n";
        return false;
    }

    ifstream in(inputName, ios::binary);

    if (!in) {
        cerr << "Error: Cannot open compressed file.\n";
        return false;
    }

    char magic[4];
    in.read(magic, 4);

    if (!in || string(magic, 4) != "HUF1") {
        cerr << "Invalid compressed file format.\n";
        return false;
    }

    uint64_t originalSize;
    in.read(reinterpret_cast<char*>(&originalSize),
            sizeof(originalSize));

    if (!in || originalSize >
        static_cast<uint64_t>(-1) / 8) {
        cerr << "Invalid or unsupported original file size.\n";
        return false;
    }

    uint64_t freq[256] = {};
    uint64_t total = 0;

    for (int i = 0; i < 256; i++) {
        in.read(reinterpret_cast<char*>(&freq[i]),
                sizeof(freq[i]));

        if (!in || freq[i] > originalSize - total) {
            cerr << "Invalid frequency table.\n";
            return false;
        }

        total += freq[i];
    }

    if (total != originalSize) {
        cerr << "Corrupted file: size does not match frequencies.\n";
        return false;
    }

    Node* root = buildTree(freq);

    ofstream out(outputName, ios::binary);

    if (!out) {
        deleteTree(root);
        cerr << "Error: Cannot create output file.\n";
        return false;
    }

    if (originalSize == 0) {
        deleteTree(root);
        cout << "\nDecompression successful! Empty file restored.\n";
        return true;
    }

    if (!root) {
        cerr << "Invalid Huffman tree.\n";
        return false;
    }

    if (root->isLeaf()) {
        for (uint64_t i = 0; i < originalSize; i++) {
            out.put(static_cast<char>(root->ch));
        }
    } else {
        Node* current = root;
        uint64_t written = 0;
        char byte;

        while (written < originalSize && in.get(byte)) {
            unsigned char bits = static_cast<unsigned char>(byte);

            for (int i = 7; i >= 0 && written < originalSize; i--) {
                bool one = (bits >> i) & 1;
                current = one ? current->right : current->left;

                if (!current) {
                    cerr << "Corrupted compressed data.\n";
                    out.close();
                    deleteTree(root);
                    return false;
                }

                if (current->isLeaf()) {
                    out.put(static_cast<char>(current->ch));
                    written++;
                    current = root;
                }
            }
        }

        if (written != originalSize) {
            cerr << "Compressed file ended unexpectedly.\n";
            out.close();
            deleteTree(root);
            return false;
        }
    }

    bool success = static_cast<bool>(out);
    out.close();
    deleteTree(root);

    if (!success) {
        cerr << "Error writing decompressed file.\n";
        return false;
    }

    cout << "\nDecompression successful!\n";
    cout << "Restored file: " << outputName << '\n';

    return true;
}

int main() {
    int choice;
    string inputFile, outputFile;

    do {
        cout << "\n====================================\n";
        cout << "     HUFFMAN FILE COMPRESSOR\n";
        cout << "====================================\n";
        cout << "1. Compress a file\n";
        cout << "2. Decompress a file\n";
        cout << "3. Exit\n";
        cout << "Enter your choice: ";

        if (!(cin >> choice)) {
            cout << "Invalid input. Exiting.\n";
            break;
        }

        if (choice == 1 || choice == 2) {
            cout << "Enter input filename: ";
            cin >> inputFile;

            cout << "Enter output filename: ";
            cin >> outputFile;

            if (choice == 1) {
                compressFile(inputFile, outputFile);
            } else {
                decompressFile(inputFile, outputFile);
            }
        } else if (choice == 3) {
            cout << "Thank you for using Huffman File Compressor!\n";
        } else {
            cout << "Invalid choice. Please try again.\n";
        }

    } while (choice != 3);

    return 0;
}