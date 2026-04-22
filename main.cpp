#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cstdio>

using namespace std;

const int MAX_INDEX_LEN = 64;
const int MAX_FILE_NAME_LEN = 64;
const string INDEX_FILE = "storage.idx";

struct IndexEntry {
    char index[MAX_INDEX_LEN + 1];
    char filename[MAX_FILE_NAME_LEN + 1];
    int size;  // number of values in this index's file
};

class FileStorage {
private:
    string indexFile;

    int findIndexInFile(const string& key, string& filename, int& size) {
        ifstream idx(indexFile, ios::binary);
        if (!idx.is_open()) return -1;

        IndexEntry entry;
        int pos = 0;
        int foundPos = -1;

        while (idx.read((char*)&entry, sizeof(IndexEntry))) {
            int cmp = strcmp(key.c_str(), entry.index);
            if (cmp == 0) {
                filename = entry.filename;
                size = entry.size;
                foundPos = pos;
                break;
            }
            pos++;
        }
        idx.close();
        return foundPos;
    }

    int findValueInFile(const string& filename, int value) {
        ifstream f(filename, ios::binary);
        if (!f.is_open()) return -1;

        int v;
        int pos = 0;
        while (f.read((char*)&v, sizeof(int))) {
            if (v == value) {
                f.close();
                return pos;
            }
            pos++;
        }
        f.close();
        return -1;
    }

    void writeValuesToFile(const string& filename, const vector<int>& values) {
        ofstream f(filename, ios::binary | ios::trunc);
        if (!f.is_open()) return;
        for (int v : values) {
            f.write((char*)&v, sizeof(int));
        }
        f.close();
    }

    void readAllValues(const string& filename, vector<int>& values) {
        ifstream f(filename, ios::binary);
        if (!f.is_open()) return;
        int v;
        while (f.read((char*)&v, sizeof(int))) {
            values.push_back(v);
        }
        f.close();
    }

    void insertIndexEntry(const string& key, const string& filename, int size) {
        vector<IndexEntry> entries;
        ifstream idx(indexFile, ios::binary);
        if (idx.is_open()) {
            IndexEntry entry;
            while (idx.read((char*)&entry, sizeof(IndexEntry))) {
                entries.push_back(entry);
            }
            idx.close();
        }

        IndexEntry newEntry;
        strncpy(newEntry.index, key.c_str(), MAX_INDEX_LEN);
        newEntry.index[MAX_INDEX_LEN] = '\0';
        strncpy(newEntry.filename, filename.c_str(), MAX_FILE_NAME_LEN);
        newEntry.filename[MAX_FILE_NAME_LEN] = '\0';
        newEntry.size = size;

        bool inserted = false;
        for (size_t i = 0; i < entries.size(); i++) {
            if (strcmp(key.c_str(), entries[i].index) < 0) {
                entries.insert(entries.begin() + i, newEntry);
                inserted = true;
                break;
            }
        }
        if (!inserted) entries.push_back(newEntry);

        ofstream outIdx(indexFile, ios::binary | ios::trunc);
        for (const auto& e : entries) {
            outIdx.write((char*)&e, sizeof(IndexEntry));
        }
        outIdx.close();
    }

    void updateIndexSize(const string& key, int newSize) {
        vector<IndexEntry> entries;
        ifstream idx(indexFile, ios::binary);
        if (idx.is_open()) {
            IndexEntry entry;
            while (idx.read((char*)&entry, sizeof(IndexEntry))) {
                entries.push_back(entry);
            }
            idx.close();
        }

        for (auto& e : entries) {
            if (strcmp(key.c_str(), e.index) == 0) {
                e.size = newSize;
                break;
            }
        }

        ofstream outIdx(indexFile, ios::binary | ios::trunc);
        for (const auto& e : entries) {
            outIdx.write((char*)&e, sizeof(IndexEntry));
        }
        outIdx.close();
    }

    void removeIndexEntry(const string& key) {
        vector<IndexEntry> entries;
        ifstream idx(indexFile, ios::binary);
        if (idx.is_open()) {
            IndexEntry entry;
            while (idx.read((char*)&entry, sizeof(IndexEntry))) {
                entries.push_back(entry);
            }
            idx.close();
        }

        auto it = entries.begin();
        while (it != entries.end()) {
            if (strcmp(key.c_str(), it->index) == 0) {
                it = entries.erase(it);
                break;
            } else {
                ++it;
            }
        }

        ofstream outIdx(indexFile, ios::binary | ios::trunc);
        for (const auto& e : entries) {
            outIdx.write((char*)&e, sizeof(IndexEntry));
        }
        outIdx.close();
    }

    string getFilename(const string& key) {
        return "idx_" + key + ".dat";
    }

public:
    FileStorage() : indexFile(INDEX_FILE) {}

    void find(const string& key) {
        string filename;
        int size;
        findIndexInFile(key, filename, size);

        if (filename.empty() || size == 0) {
            cout << "null" << endl;
            return;
        }

        vector<int> values;
        readAllValues(filename, values);

        if (values.empty()) {
            cout << "null" << endl;
            return;
        }

        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0) cout << " ";
            cout << values[i];
        }
        cout << endl;
    }

    void insert(const string& key, int value) {
        string filename;
        int size;
        findIndexInFile(key, filename, size);

        if (filename.empty()) {
            filename = getFilename(key);
            ofstream f(filename, ios::binary | ios::trunc);
            f.write((char*)&value, sizeof(int));
            f.close();
            insertIndexEntry(key, filename, 1);
            return;
        }

        vector<int> values;
        readAllValues(filename, values);

        for (int v : values) {
            if (v == value) return;
        }

        values.push_back(value);
        sort(values.begin(), values.end());
        writeValuesToFile(filename, values);
        updateIndexSize(key, values.size());
    }

    void remove(const string& key, int value) {
        string filename;
        int size;
        findIndexInFile(key, filename, size);

        if (filename.empty()) return;

        vector<int> values;
        readAllValues(filename, values);

        auto it = values.begin();
        while (it != values.end()) {
            if (*it == value) {
                it = values.erase(it);
                break;
            } else {
                ++it;
            }
        }

        if (values.empty()) {
            removeIndexEntry(key);
            std::remove(filename.c_str());
        } else {
            writeValuesToFile(filename, values);
            updateIndexSize(key, values.size());
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
