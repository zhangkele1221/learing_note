// https://www.cnblogs.com/whuanle/p/16297025.html#%E8%AE%BE%E8%AE%A1%E6%80%9D%E8%B7%AF
/*
https://www.cnblogs.com/whuanle/p/16297025.html#%E8%AE%BE%E8%AE%A1%E6%80%9D%E8%B7%AF

实现一个最简单的 LSM（Log-Structured Merge-tree）数据库的基础版本涉及几个关键部分：
内存表（MemTable），磁盘表（SSTable），以及合并和压缩机制。
下面是一个简化的示例，它提供了一个基本框架，但请注意，为了简洁和清晰，这个实现省略了很多实用功能，
如错误处理、持久化、并发控制等。

这个代码实现了一个非常基本的 LSM 数据库，包括一个内存表（MemTable）和多个磁盘表（SSTable）。
它支持基本的 put、get 和 del 操作。当内存表达到一定大小时，它会被刷新到一个新的 SSTable。
*/

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

class MemTable {
private:
    std::map<std::string, std::string> table;

public:
    void put(const std::string& key, const std::string& value) {
        table[key] = value;
    }

    bool get(const std::string& key, std::string& value) {
        auto it = table.find(key);
        if (it != table.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    void del(const std::string& key) {
        table.erase(key);
    }

    bool isEmpty() const {
        return table.empty();
    }

    size_t size() const {
        return table.size();
    }

    void dumpToSSTable(std::ofstream& outFile) {
        for (const auto& pair : table) {
            outFile << pair.first << ":" << pair.second << "\n";
        }
        table.clear();
    }
};

class LSMDB {
private:
    MemTable memtable;
    std::vector<std::string> ssTables;
    const size_t SOME_THRESHOLD = 10;  // Threshold for flushing MemTable to SSTable

    void flushMemTable() {
        std::string fileName = "sst_" + std::to_string(ssTables.size()) + ".txt";
        std::ofstream outFile(fileName);
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << fileName << std::endl;
            return;
        }
        memtable.dumpToSSTable(outFile);
        outFile.close();
        ssTables.push_back(fileName);
    }

public:
    void put(const std::string& key, const std::string& value) {
        memtable.put(key, value);

        if (memtable.size() >= SOME_THRESHOLD) {
            flushMemTable();
        }
    }

    bool get(const std::string& key, std::string& value) {
        if (memtable.get(key, value)) {
            return true;
        }

        for (const auto& tableName : ssTables) {
            std::ifstream inFile(tableName);
            if (!inFile.is_open()) {
                std::cerr << "Failed to open file for reading: " << tableName << std::endl;
                continue;
            }

            std::string line;
            while (std::getline(inFile, line)) {
                std::istringstream lineStream(line);
                std::string curKey;
                std::string curValue;
                if (std::getline(lineStream, curKey, ':') && std::getline(lineStream, curValue)) {
                    if (curKey == key) {
                        value = curValue;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void del(const std::string& key) {
        memtable.del(key);
    }
};

int main() {
    LSMDB db;
    db.put("key1", "value1");
    db.put("key2", "value2");

    std::string value;
    if (db.get("key1", value)) {
        std::cout << "key1: " << value << std::endl;
    } else {
        std::cout << "key1 not found." << std::endl;
    }

    return 0;
}
