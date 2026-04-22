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
    int next;  // next entry with same key (for duplicate keys)
    int prev;  // previous entry (for deletion)
};

struct IndexBlock {
    IndexEntry entries[INDEX_BLOCK_SIZE];
    int count;
    int nextBlock;
    int freeHead;  // head of free list
};

class FileStorage {
private:
    string indexFile;
    string dataFile;

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
        block.freeHead = -1;

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
            block.freeHead = -1;
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
        newBlock.freeHead = -1;

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

    void appendValues(const int* values, int count, int& offset) {
        ofstream data(dataFile, ios::binary | ios::app);
        if (data.is_open()) {
            offset = data.tellp();
            data.write((char*)values, count * sizeof(int));
            data.close();
        }
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

        appendValues(values, count + 1, *newOffset);
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
            *newOffset = -1;
            *newCount = 0;
        } else {
            appendValues(values, count, *newOffset);
            *newCount = count;
        }

        delete[] values;
    }

public:
    FileStorage() : indexFile(INDEX_FILE), dataFile(DATA_FILE) {}

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
            appendValues(values, 1, newOffset);
            delete[] values;
            newCount = 1;

            IndexEntry newEntry = {};
            strncpy(newEntry.index, key.c_str(), MAX_INDEX_LEN);
            newEntry.index[MAX_INDEX_LEN] = '\0';
            newEntry.offset = newOffset;
            newEntry.count = newCount;
            newEntry.next = -1;
            newEntry.prev = -1;

            insertEntry(newEntry);
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
