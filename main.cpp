#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <cstdio>

using namespace std;

const int MAX_INDEX_LEN = 64;
const string INDEX_FILE = "storage.idx";
const string DATA_FILE = "storage.dat";

const int INDEX_BLOCK_SIZE = 64;
const int MAX_INDEX_BLOCKS = 2000;

struct IndexEntry {
    char index[MAX_INDEX_LEN + 1];
    int offset;
    int count;
};

struct IndexBlock {
    IndexEntry entries[INDEX_BLOCK_SIZE];
    int count;
    int nextBlock;
};

struct DataHeader {
    int freeHead;
    int fileSize;
};

class FileStorage {
private:
    string indexFile;
    string dataFile;

    void initFiles() {
        ifstream idx(indexFile, ios::binary | ios::ate);
        if (!idx.is_open() || idx.tellg() == 0) {
            idx.close();
            ofstream outIdx(indexFile, ios::binary | ios::trunc);
            outIdx.close();
        } else {
            idx.close();
        }

        ifstream data(dataFile, ios::binary | ios::ate);
        if (!data.is_open() || data.tellg() == 0) {
            data.close();
            DataHeader header = {};
            header.freeHead = -1;
            header.fileSize = sizeof(DataHeader);
            ofstream outData(dataFile, ios::binary | ios::trunc);
            outData.write((char*)&header, sizeof(DataHeader));
            outData.close();
        } else {
            data.close();
        }
    }

    DataHeader readDataHeader() {
        DataHeader header = {};
        ifstream data(dataFile, ios::binary);
        if (data.is_open()) {
            data.read((char*)&header, sizeof(DataHeader));
            data.close();
        }
        return header;
    }

    void writeDataHeader(const DataHeader& header) {
        fstream data(dataFile, ios::binary | ios::in | ios::out);
        if (data.is_open()) {
            data.seekp(0, ios::beg);
            data.write((char*)&header, sizeof(DataHeader));
            data.close();
        }
    }

    int getIndexBlockCount() {
        ifstream idx(indexFile, ios::binary | ios::ate);
        if (!idx.is_open()) return 0;
        int count = idx.tellg() / sizeof(IndexBlock);
        idx.close();
        return count;
    }

    IndexBlock readIndexBlock(int blockNum) {
        IndexBlock block = {};
        block.count = 0;
        block.nextBlock = -1;

        ifstream idx(indexFile, ios::binary);
        if (idx.is_open()) {
            idx.seekg(blockNum * sizeof(IndexBlock), ios::beg);
            idx.read((char*)&block, sizeof(IndexBlock));
            idx.close();
        }
        return block;
    }

    void writeIndexBlock(int blockNum, const IndexBlock& block) {
        fstream idx(indexFile, ios::binary | ios::in | ios::out);
        if (idx.is_open()) {
            idx.seekp(blockNum * sizeof(IndexBlock), ios::beg);
            idx.write((char*)&block, sizeof(IndexBlock));
            idx.close();
        }
    }

    int appendIndexBlock(const IndexBlock& block) {
        ofstream idx(indexFile, ios::binary | ios::app);
        int blockNum = idx.tellp() / sizeof(IndexBlock);
        idx.write((char*)&block, sizeof(IndexBlock));
        idx.close();
        return blockNum;
    }

    int findEntryPos(const string& key, int& blockNum, int& entryPos) {
        int numBlocks = getIndexBlockCount();
        if (numBlocks == 0) return -1;

        blockNum = 0;
        while (blockNum != -1) {
            IndexBlock block = readIndexBlock(blockNum);
            for (int i = 0; i < block.count; i++) {
                int cmp = strcmp(key.c_str(), block.entries[i].index);
                if (cmp == 0) {
                    entryPos = i;
                    return blockNum;
                }
                if (cmp < 0) {
                    entryPos = i;
                    return -1;
                }
            }
            blockNum = block.nextBlock;
        }
        return -1;
    }

    int findInsertPos(const string& key, int& blockNum, int& entryPos) {
        int numBlocks = getIndexBlockCount();
        if (numBlocks == 0) {
            blockNum = -1;
            entryPos = 0;
            return -1;
        }

        blockNum = 0;
        int prevBlock = -1;
        while (blockNum != -1) {
            IndexBlock block = readIndexBlock(blockNum);
            int insertPos = 0;
            while (insertPos < block.count && strcmp(key.c_str(), block.entries[insertPos].index) > 0) {
                insertPos++;
            }

            if (insertPos < block.count) {
                entryPos = insertPos;
                return blockNum;
            }

            prevBlock = blockNum;
            blockNum = block.nextBlock;
        }

        blockNum = prevBlock;
        IndexBlock block = readIndexBlock(blockNum);
        entryPos = block.count;
        return blockNum;
    }

    void insertEntry(const IndexEntry& entry) {
        int blockNum, entryPos;
        int foundBlock = findEntryPos(entry.index, blockNum, entryPos);

        if (foundBlock != -1) {
            return;
        }

        int insertBlockNum, insertPos;
        findInsertPos(entry.index, insertBlockNum, insertPos);

        if (insertBlockNum == -1) {
            IndexBlock block = {};
            block.count = 1;
            block.nextBlock = -1;
            block.entries[0] = entry;
            appendIndexBlock(block);
            return;
        }

        IndexBlock block = readIndexBlock(insertBlockNum);

        if (block.count < INDEX_BLOCK_SIZE) {
            for (int i = block.count; i > insertPos; i--) {
                block.entries[i] = block.entries[i - 1];
            }
            block.entries[insertPos] = entry;
            block.count++;
            writeIndexBlock(insertBlockNum, block);
            return;
        }

        int splitPos = INDEX_BLOCK_SIZE / 2;
        IndexBlock newBlock = {};
        newBlock.count = 0;
        newBlock.nextBlock = block.nextBlock;

        for (int i = splitPos; i < INDEX_BLOCK_SIZE; i++) {
            newBlock.entries[newBlock.count++] = block.entries[i];
        }
        block.count = splitPos;

        if (insertPos <= splitPos) {
            for (int i = block.count; i > insertPos; i--) {
                block.entries[i] = block.entries[i - 1];
            }
            block.entries[insertPos] = entry;
            block.count++;
        } else {
            int newPos = insertPos - splitPos;
            for (int i = newBlock.count; i > newPos; i--) {
                newBlock.entries[i] = newBlock.entries[i - 1];
            }
            newBlock.entries[newPos] = entry;
            newBlock.count++;
        }

        int newBlockNum = appendIndexBlock(newBlock);
        block.nextBlock = newBlockNum;
        writeIndexBlock(insertBlockNum, block);
    }

    void deleteEntry(const string& key) {
        int blockNum, entryPos;
        int foundBlock = findEntryPos(key, blockNum, entryPos);

        if (foundBlock == -1) return;

        IndexBlock block = readIndexBlock(foundBlock);
        for (int i = entryPos; i < block.count - 1; i++) {
            block.entries[i] = block.entries[i + 1];
        }
        block.count--;
        writeIndexBlock(foundBlock, block);
    }

    IndexEntry findEntry(const string& key) {
        int blockNum, entryPos;
        int foundBlock = findEntryPos(key, blockNum, entryPos);

        if (foundBlock == -1) {
            IndexEntry empty = {};
            empty.count = 0;
            return empty;
        }

        IndexBlock block = readIndexBlock(foundBlock);
        return block.entries[entryPos];
    }

    void updateEntry(const string& key, int newOffset, int newCount) {
        int blockNum, entryPos;
        int foundBlock = findEntryPos(key, blockNum, entryPos);

        if (foundBlock == -1) return;

        IndexBlock block = readIndexBlock(foundBlock);
        block.entries[entryPos].offset = newOffset;
        block.entries[entryPos].count = newCount;
        writeIndexBlock(foundBlock, block);
    }

    void readValues(int offset, int count, int* values) {
        ifstream data(dataFile, ios::binary);
        if (data.is_open()) {
            data.seekg(offset, ios::beg);
            data.read((char*)values, count * sizeof(int));
            data.close();
        }
    }

    void writeValues(int offset, const int* values, int count) {
        fstream data(dataFile, ios::binary | ios::in | ios::out);
        if (data.is_open()) {
            data.seekp(offset, ios::beg);
            data.write((char*)values, count * sizeof(int));
            data.close();
        }
    }

    int allocateDataSpace(int size) {
        DataHeader header = readDataHeader();

        int prevFree = -1;
        int currFree = header.freeHead;

        while (currFree != -1) {
            struct FreeBlock {
                int offset;
                int size;
                int next;
            } freeBlock;

            ifstream data(dataFile, ios::binary);
            data.seekg(currFree, ios::beg);
            data.read((char*)&freeBlock, sizeof(FreeBlock));
            data.close();

            if (freeBlock.size >= size) {
                if (prevFree == -1) {
                    header.freeHead = freeBlock.next;
                } else {
                    struct FreeBlock prev;
                    ifstream data2(dataFile, ios::binary);
                    data2.seekg(prevFree, ios::beg);
                    data2.read((char*)&prev, sizeof(FreeBlock));
                    data2.close();
                    prev.next = freeBlock.next;
                    writeValues(prevFree, (int*)&prev, 2);
                }
                writeDataHeader(header);
                return currFree;
            }

            prevFree = currFree;
            currFree = freeBlock.next;
        }

        int offset = header.fileSize;
        header.fileSize += size + 12;
        writeDataHeader(header);

        int negOne = -1;
        fstream data(dataFile, ios::binary | ios::in | ios::out);
        data.seekp(offset, ios::beg);
        data.write((char*)&size, sizeof(int));
        data.write((char*)&negOne, sizeof(int));
        data.write((char*)&negOne, sizeof(int));
        data.close();

        return offset + 12;
    }

    void freeDataSpace(int offset, int size) {
        DataHeader header = readDataHeader();

        struct FreeBlock {
            int offset;
            int size;
            int next;
        } freeBlock;
        freeBlock.offset = offset;
        freeBlock.size = size;
        freeBlock.next = header.freeHead;

        writeValues(offset, (int*)&freeBlock, 3);

        header.freeHead = offset;
        writeDataHeader(header);
    }

    int findValuePos(int offset, int count, int value) {
        int* values = new int[count];
        readValues(offset, count, values);

        int left = 0, right = count - 1;
        int result = -1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (values[mid] == value) {
                result = mid;
                break;
            }
            if (values[mid] < value) left = mid + 1;
            else right = mid - 1;
        }

        delete[] values;
        return result;
    }

    void insertValue(int offset, int count, int value, int* newOffset, int* newCount) {
        int* values = new int[count + 1];
        readValues(offset, count, values);

        int pos = 0;
        while (pos < count && values[pos] < value) pos++;

        for (int i = count; i > pos; i--) values[i] = values[i - 1];
        values[pos] = value;

        int newSize = (count + 1) * sizeof(int);
        int newOff = allocateDataSpace(newSize);
        writeValues(newOff, values, count + 1);
        freeDataSpace(offset, count * sizeof(int));

        *newOffset = newOff;
        *newCount = count + 1;

        delete[] values;
    }

    void deleteValue(int offset, int count, int value, int* newOffset, int* newCount) {
        int* values = new int[count];
        readValues(offset, count, values);

        int pos = 0;
        while (pos < count && values[pos] != value) pos++;

        if (pos < count) {
            for (int i = pos; i < count - 1; i++) values[i] = values[i + 1];
            count--;
        }

        if (count == 0) {
            freeDataSpace(offset, 0);
            *newOffset = -1;
            *newCount = 0;
        } else {
            writeValues(offset, values, count);
            freeDataSpace(offset + count * sizeof(int), sizeof(int));
            *newOffset = offset;
            *newCount = count;
        }

        delete[] values;
    }

public:
    FileStorage() : indexFile(INDEX_FILE), dataFile(DATA_FILE) {
        initFiles();
    }

    void find(const string& key) {
        IndexEntry entry = findEntry(key);
        if (entry.count == 0) {
            printf("null\n");
            return;
        }

        int* values = new int[entry.count];
        readValues(entry.offset, entry.count, values);

        for (int i = 0; i < entry.count; i++) {
            if (i > 0) printf(" ");
            printf("%d", values[i]);
        }
        printf("\n");

        delete[] values;
    }

    void insert(const string& key, int value) {
        IndexEntry entry = findEntry(key);
        int newOffset, newCount;

        if (entry.count == 0) {
            int* values = new int[1];
            values[0] = value;
            int newOff = allocateDataSpace(sizeof(int));
            writeValues(newOff, values, 1);
            delete[] values;
            newCount = 1;

            IndexEntry newEntry = {};
            strncpy(newEntry.index, key.c_str(), MAX_INDEX_LEN);
            newEntry.index[MAX_INDEX_LEN] = '\0';
            newEntry.offset = newOff;
            newEntry.count = newCount;

            int insertBlockNum, insertPos;
            findInsertPos(newEntry.index, insertBlockNum, insertPos);
            if (insertBlockNum == -1) {
                IndexBlock block = {};
                block.count = 1;
                block.nextBlock = -1;
                block.entries[0] = newEntry;
                appendIndexBlock(block);
            } else {
                IndexBlock block = readIndexBlock(insertBlockNum);
                if (block.count < INDEX_BLOCK_SIZE) {
                    for (int i = block.count; i > insertPos; i--) {
                        block.entries[i] = block.entries[i - 1];
                    }
                    block.entries[insertPos] = newEntry;
                    block.count++;
                    writeIndexBlock(insertBlockNum, block);
                } else {
                    int splitPos = INDEX_BLOCK_SIZE / 2;
                    IndexBlock newBlock = {};
                    newBlock.count = 0;
                    newBlock.nextBlock = block.nextBlock;

                    for (int i = splitPos; i < INDEX_BLOCK_SIZE; i++) {
                        newBlock.entries[newBlock.count++] = block.entries[i];
                    }
                    block.count = splitPos;

                    if (insertPos <= splitPos) {
                        for (int i = block.count; i > insertPos; i--) {
                            block.entries[i] = block.entries[i - 1];
                        }
                        block.entries[insertPos] = newEntry;
                        block.count++;
                    } else {
                        int newPos = insertPos - splitPos;
                        for (int i = newBlock.count; i > newPos; i--) {
                            newBlock.entries[i] = newBlock.entries[i - 1];
                        }
                        newBlock.entries[newPos] = newEntry;
                        newBlock.count++;
                    }

                    int newBlockNum = appendIndexBlock(newBlock);
                    block.nextBlock = newBlockNum;
                    writeIndexBlock(insertBlockNum, block);
                }
            }
        } else {
            int valuePos = findValuePos(entry.offset, entry.count, value);
            if (valuePos != -1) return;

            insertValue(entry.offset, entry.count, value, &newOffset, &newCount);
            updateEntry(key, newOffset, newCount);
        }
    }

    void remove(const string& key, int value) {
        IndexEntry entry = findEntry(key);
        if (entry.count == 0) return;

        int newOffset, newCount;
        deleteValue(entry.offset, entry.count, value, &newOffset, &newCount);

        if (newCount == 0) {
            deleteEntry(key);
        } else {
            updateEntry(key, newOffset, newCount);
        }
    }
};

int main() {
    int n;
    scanf("%d", &n);

    FileStorage storage;

    for (int i = 0; i < n; i++) {
        char cmd[20];
        scanf("%s", cmd);

        if (strcmp(cmd, "insert") == 0) {
            char key[MAX_INDEX_LEN + 1];
            int value;
            scanf("%s%d", key, &value);
            storage.insert(key, value);
        } else if (strcmp(cmd, "delete") == 0) {
            char key[MAX_INDEX_LEN + 1];
            int value;
            scanf("%s%d", key, &value);
            storage.remove(key, value);
        } else if (strcmp(cmd, "find") == 0) {
            char key[MAX_INDEX_LEN + 1];
            scanf("%s", key);
            storage.find(key);
        }
    }

    return 0;
}
