#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <algorithm>

constexpr int DIM = 128;       // 向量维度
constexpr int NUM_DATA = 1000000; // 数据集大小
constexpr int NUM_QUERY = 10;   // 查询次数
constexpr int M = 4;           // PQ 子空间数
constexpr int K = 256;         // PQ 每子空间聚类数

// 生成随机向量
std::vector<float> generate_random_vector(int dim) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    std::vector<float> vec(dim);
    for (auto& v : vec) {
        v = dist(gen);
    }
    return vec;
}

// 暴力线性扫描最近邻搜索函数
// 参数：
//   query: 待查询点的特征向量（DIM维）
//   dataset: 数据集容器，包含多个DIM维特征向量
//   nearest_idx: 输出参数，最近邻在数据集中的索引
//   min_dist: 输出参数，与最近邻的平方距离（未开根号）
void linear_scan(const std::vector<float>& query, 
                const std::vector<std::vector<float>>& dataset,
                int& nearest_idx, float& min_dist) 
{
    // 初始化最小距离为float最大值，确保首次比较必定更新
    min_dist = std::numeric_limits<float>::max();
    // 初始化最近邻索引为-1（表示无效索引/未找到）
    nearest_idx = -1;

    // 遍历数据集中的所有样本点
    for (size_t i = 0; i < dataset.size(); ++i) {
        float dist = 0.0f; // 当前样本点与查询点的平方距离
        
        // 逐维度计算欧氏距离的平方（避免开根号计算）
        // 注意：DIM需为预定义常量，表示特征维度数
        for (int j = 0; j < DIM; ++j) {
            // 计算当前维度差值
            float diff = query[j] - dataset[i][j];
            // 累加平方差值（等价于欧式距离平方）
            dist += diff * diff;
        }

        // 如果找到更近的邻居，更新记录
        if (dist < min_dist) {
            min_dist = dist;    // 更新最小距离
            nearest_idx = i;    // 更新最近邻索引
        }
    }

    // 注意：
    // 1. 当数据集为空时，nearest_idx保持-1
    // 2. 返回的是平方距离，实际使用时可能需要开根号
    // 3. 假设所有数据点维度均为DIM且与query维度一致
}


// PQ 量化结构
struct PQIndex {
    std::vector<std::vector<std::vector<float>>> codebooks; // [M][K][DIM/M]
    std::vector<std::vector<uint8_t>> codes;                // 编码后的数据
    
    void train(const std::vector<std::vector<float>>& dataset) { /* 简化的随机码本 */ 
        const int sub_dim = DIM / M;//128/4
        // constexpr int K = 256;         //m 是子空间数量  k 是 PQ 每子空间聚类数
        codebooks.resize(M, std::vector<std::vector<float>>(K, std::vector<float>(sub_dim)));
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (auto& cb : codebooks) {
            for (auto& center : cb) {
                for (auto& v : center) v = dist(gen);
            }
        }
    }
    
    void encode(const std::vector<std::vector<float>>& dataset) {
        const int sub_dim = DIM / M;
        codes.resize(dataset.size(), std::vector<uint8_t>(M));
        
        for (size_t i = 0; i < dataset.size(); ++i) {
            for (int m = 0; m < M; ++m) {
                int start = m * sub_dim;
                float min_dist = std::numeric_limits<float>::max();
                for (int k = 0; k < K; ++k) {
                    float dist = 0.0f;
                    for (int d = 0; d < sub_dim; ++d) {
                        float diff = dataset[i][start+d] - codebooks[m][k][d];
                        dist += diff * diff;
                    }
                    if (dist < min_dist) {
                        min_dist = dist;
                        codes[i][m] = k;
                    }
                }
            }
        }
    }
    
    void search(const std::vector<float>& query, 
               int& nearest_idx, float& min_dist) {
        std::vector<std::vector<float>> precomputed(M, std::vector<float>(K));
        const int sub_dim = DIM / M;
        
        // 预计算距离表
        for (int m = 0; m < M; ++m) {
            int start = m * sub_dim;
            for (int k = 0; k < K; ++k) {
                float dist = 0.0f;
                for (int d = 0; d < sub_dim; ++d) {
                    float diff = query[start+d] - codebooks[m][k][d];
                    dist += diff * diff;
                }
                precomputed[m][k] = dist;
            }
        }
        
        // 搜索最近邻
        min_dist = std::numeric_limits<float>::max();
        nearest_idx = -1;
        for (size_t i = 0; i < codes.size(); ++i) {
            float dist = 0.0f;
            for (int m = 0; m < M; ++m) {
                dist += precomputed[m][codes[i][m]];
            }
            if (dist < min_dist) {
                min_dist = dist;
                nearest_idx = i;
            }
        }
    }
};

int main() {
    // 生成测试数据
    std::vector<std::vector<float>> dataset(NUM_DATA);
    for (auto& vec : dataset) {
        vec = generate_random_vector(DIM);
    }
    
    // 生成查询
    std::vector<std::vector<float>> queries(NUM_QUERY);
    for (auto& q : queries) {
        q = generate_random_vector(DIM);
    }
    
    // 测试暴力检索
    {
        // 暴力搜索（线性扫描）测试块开始
        int correct = 0; // 记录正确匹配的查询数量
        auto start = std::chrono::high_resolution_clock::now(); // 记录测试开始时间点

        // 遍历所有查询点
        for (const auto& q : queries) {
            int idx;        // 用于接收最近邻的索引
            float dist;     // 用于接收最近邻的距离
            // 执行线性扫描搜索：在数据集中寻找查询点q的最近邻
            linear_scan(q, dataset, idx, dist);
            // 简单正确性检查：索引为-1表示无效结果（通常表示数据集为空或搜索失败）
            // 若索引有效则计入正确匹配
            correct += (idx != -1);
        }

        auto end = std::chrono::high_resolution_clock::now(); // 记录测试结束时间点
        // 计算并输出暴力搜索耗时及正确率统计
        std::cout << "Brute-force: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() 
                << "ms | Correct: " << correct << "/" << NUM_QUERY << std::endl;
        // 注意：此处正确性验证仅检查返回结果是否有效（非-1）
    } // 暴力搜索测试块结束（自动释放内部变量）

    
    // 测试 PQ
    {
        PQIndex pq;
        pq.train(dataset);// 这个测试不合理这里少了聚类的计算过程   真实的情况是在离线训练出来 pq的聚类和 编码  所以这里只能看看
        pq.encode(dataset);
        
        int correct = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (const auto& q : queries) {
            int idx;
            float dist;
            pq.search(q, idx, dist);
            correct += (idx != -1); // 简单正确性验证
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "PQ: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() 
                  << "ms | Correct: " << correct << "/" << NUM_QUERY << std::endl;
    }
    
    return 0;
}

//原始代码的 PQ 耗时短是假象：因省略了聚类，导致在线搜索速度“虚快”，但结果几乎不可用。
//真实场景中的 PQ：离线聚类虽增加训练时间，但在线搜索仍比暴力快几个数量级，且召回率高。
//优化方向：通过 IVFPQ、SIMD、近似聚类等技术，平衡速度与精度。

推荐系统：用户特征向量检索时，离线训练 PQ 码本，在线快速查找相似用户。
图像检索：提取图片特征后离线编码，在线用 PQ 加速搜索。
流数据更新：定期（如每天）增量更新码本，避免完全重新训练。


g++ -O3 -std=c++11 -march=native pq_test.cpp -o pq_test

关键结论
永远不要在线聚类：离线训练是 PQ 高效性的前提。
正确性验证：用户代码中的 correct 变量仅检查是否找到索引，实际应验证 PQ 结果与暴力扫描的一致性。
优化方向：使用 SIMD 指令加速距离计算、引入倒排索引（IVFPQ）进一步压缩搜索空间。
通过这种离线/在线分离的设计，PQ 才能在保持高准确率的同时，实现比暴力扫描快几个数量级的检索速度。
