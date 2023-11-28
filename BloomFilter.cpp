
#include <vector>
#include <bitset>
#include <cmath>
#include <functional>

class BloomFilter {
private:
    std::bitset<1000> bitArray; // 选择合适的大小，这里只是示例
    int hashCount; // 哈希函数的数量

public:
    BloomFilter(int hashes) : hashCount(hashes) {}

    // 一个简单的哈希函数，实际应用中可能需要更复杂的哈希函数来减少碰撞
    size_t hash(const std::string& key, int seed) {
        std::hash<std::string> hasher;
        return hasher(key + std::to_string(seed)) % bitArray.size();
    }

    // 向Bloom过滤器中添加元素
    void add(const std::string& key) {
        for (int i = 0; i < hashCount; ++i) {
            bitArray.set(hash(key, i));
        }
    }

    // 检查元素是否可能在集合中
    bool possiblyContains(const std::string& key) {
        for (int i = 0; i < hashCount; ++i) {
            if (!bitArray.test(hash(key, i))) {
                return false; // 如果有一个位不是1，则元素一定不在集合中
            }
        }
        return true; // 可能在集合中，但需要进一步检查
    }
};

int main() {
    BloomFilter filter(7); // 使用7个哈希函数

    // 添加一些关键词
    filter.add("apple");
    filter.add("banana");
    filter.add("orange");

    // 检查关键词是否可能存在
    if (filter.possiblyContains("banana")) {
        std::cout << "Banana might be in the set." << std::endl;
    } else {
        std::cout << "Banana is definitely not in the set." << std::endl;
    }

    if (filter.possiblyContains("grape")) {
        std::cout << "Grape might be in the set." << std::endl;
    } else {
        std::cout << "Grape is definitely not in the set." << std::endl;
    }

    return 0;
}
