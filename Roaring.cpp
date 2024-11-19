
/*
    git clone https://github.com/RoaringBitmap/CRoaring.git
    cd CRoaring
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
*/

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include "roaring.hh"

// 分词函数，将输入文本分割为单独的单词
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// 构建倒排索引
std::unordered_map<std::string, Roaring> buildInvertedIndex(
    const std::vector<std::string>& documents) {
    std::unordered_map<std::string, Roaring> invertedIndex;

    for (uint32_t docID = 0; docID < documents.size(); ++docID) {
        std::vector<std::string> terms = tokenize(documents[docID]);
        for (const auto& term : terms) {
            invertedIndex[term].add(docID);
        }
    }

    return invertedIndex;
}

// 打印倒排索引
void printInvertedIndex(const std::unordered_map<std::string, Roaring>& index) {
    for (const auto& entry : index) {
        std::cout << "Term: " << entry.first << " -> Docs: ";
        entry.second.toUint32Array().forEach([](uint32_t value) { std::cout << value << " "; });
        std::cout << std::endl;
    }
}

int main() {
    // 模拟一些文档
    std::vector<std::string> documents = {
        "the quick brown fox",
        "jumped over the lazy dog",
        "hello world",
        "quick brown dog"
    };

    // 构建倒排索引
    std::unordered_map<std::string, Roaring> invertedIndex = buildInvertedIndex(documents);

    // 打印倒排索引
    printInvertedIndex(invertedIndex);

    // 执行 AND 查询 ("quick" AND "brown")
    if (invertedIndex.find("quick") != invertedIndex.end() && invertedIndex.find("brown") != invertedIndex.end()) {
        Roaring andResult = invertedIndex["quick"] & invertedIndex["brown"];
        
        std::cout << "AND Query Result for 'quick' AND 'brown': ";
        andResult.toUint32Array().forEach([](uint32_t value) { std::cout << value << " "; });
        std::cout << std::endl;
    }

    // 执行 OR 查询 ("quick" OR "hello")
    if (invertedIndex.find("quick") != invertedIndex.end() && invertedIndex.find("hello") != invertedIndex.end()) {
        Roaring orResult = invertedIndex["quick"] | invertedIndex["hello"];
        
        std::cout << "OR Query Result for 'quick' OR 'hello': ";
        orResult.toUint32Array().forEach([](uint32_t value) { std::cout << value << " "; });
        std::cout << std::endl;
    }

    return 0;
}



