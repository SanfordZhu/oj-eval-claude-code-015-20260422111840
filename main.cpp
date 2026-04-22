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

struct IndexEntry {
    char index[MAX_INDEX_LEN + 1];
    int offset;
    int count;
};

class FileStorage {
private:
    string indexFile;
    string dataFile;

    int getIndexEntryCount() {
        ifstream idx(indexFile, ios::binary | ios::ate);
        if (!idx.is_open()) return 0;
        int count = idx.tellg() / sizeof(IndexEntry);
        idx.close();
        return count;
    }

    IndexEntry readIndexEntry(int pos) {
        IndexEntry entry = {};
        ifstream idx(indexFile, ios::binary);
        if (idx.is_open()) {
            idx.seekg(pos * sizeof(IndexEntry), ios::beg);
            idx.read((char*)&entry, sizeof(IndexEntry));
            idx.close();
        }
        return entry;
    }

    void writeIndexEntry(int pos, const IndexEntry& entry) {
        fstream idx(indexFile, ios::binary | ios::in | ios::out);
        if (idx.is_open()) {
            idx.seekp(pos * sizeof(IndexEntry), ios::beg);
            idx.write((char*)&entry, sizeof(IndexEntry));
            idx.close();
        }
    }

    void rewriteIndexFile(const IndexEntry* entries, int count) {
        ofstream idx(indexFile, ios::binary | ios::trunc);
        if (idx.is_open()) {
            idx.write((char*)entries, count * sizeof(IndexEntry));
            idx.close();
        }
    }

    int findIndexPos(const string& key) {
        int count = getIndexEntryCount();
        if (count == 0) return -1;

        int left = 0, right = count - 1;
        while (left <= right) {
            int mid = left + (right - left) / 2;
            IndexEntry entry = readIndexEntry(mid);
            int cmp = strcmp(key.c_str(), entry.index);
            if (cmp == 0) return mid;
            if (cmp < 0) right = mid - 1;
            else left = mid + 1;
        }
        return -1;
    }

    int findInsertPos(const string& key) {
        int count = getIndexEntryCount();
        if (count == 0) return 0;

        int left = 0, right = count;
        while (left < right) {
            int mid = left + (right - left) / 2;
            IndexEntry entry = readIndexEntry(mid);
            if (strcmp(key.c_str(), entry.index) <= 0) right = mid;
            else left = mid + 1;
        }
        return left;
    }

    void insertIndexEntry(const IndexEntry& entry) {
        int pos = findInsertPos(entry.index);
        int count = getIndexEntryCount();

        IndexEntry* entries = new IndexEntry[count + 1];
        for (int i = 0; i < pos; i++) {
            entries[i] = readIndexEntry(i);
        }
        entries[pos] = entry;
        for (int i = pos; i < count; i++) {
            entries[i + 1] = readIndexEntry(i);
        }

        rewriteIndexFile(entries, count + 1);
        delete[] entries;
    }

    void deleteIndexEntry(int pos) {
        int count = getIndexEntryCount();
        if (count <= 1) {
            std::remove(indexFile.c_str());
            return;
        }

        IndexEntry* entries = new IndexEntry[count - 1];
        for (int i = 0; i < pos; i++) {
            entries[i] = readIndexEntry(i);
        }
        for (int i = pos; i < count - 1; i++) {
            entries[i] = readIndexEntry(i + 1);
        }

        rewriteIndexFile(entries, count - 1);
        delete[] entries;
    }

    IndexEntry findEntry(const string& key) {
        int pos = findIndexPos(key);
        if (pos == -1) {
            IndexEntry empty = {};
            empty.count = 0;
            return empty;
        }
        return readIndexEntry(pos);
    }

    void updateEntry(const string& key, int newOffset, int newCount) {
        int pos = findIndexPos(key);
        if (pos == -1) return;

        IndexEntry entry = readIndexEntry(pos);
        entry.offset = newOffset;
        entry.count = newCount;
        writeIndexEntry(pos, entry);
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

            insertIndexEntry(newEntry);
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
            deleteIndexEntry(findIndexPos(key));
        } else {
            updateEntry(key, newOffset, newCount);
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;

    FileStorage storage;

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string key;
            int value;
            cin >> key >> value;
            storage.insert(key, value);
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            storage.remove(key, value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            storage.find(key);
        }
    }

    return 0;
}
