
#include <iostream>
#include <vector>
#include <cmath>
#include <immintrin.h> // AVX2 intrinsics

// 简化示例：演示 PForDelta vs PEF+SIMD 解码对比

using namespace std;

// ------------- PForDelta ----------------

// 简单固定宽度编码（bit_width <= 16）
pair<vector<uint16_t>, vector<uint16_t>> pfordelta_encode(const vector<uint32_t>& deltas, int bit_width) {
    uint32_t mask = (1u << bit_width) - 1;
    vector<uint16_t> main_bits;
    vector<uint16_t> exceptions;
    main_bits.reserve(deltas.size());
    exceptions.reserve(deltas.size());
    for (auto d : deltas) {
        if (d <= mask) {
            main_bits.push_back((uint16_t)d);
            exceptions.push_back(0);
        } else {
            main_bits.push_back(0);
            exceptions.push_back((uint16_t)d);
        }
    }
    return {main_bits, exceptions};
}

vector<uint32_t> pfordelta_decode(const vector<uint16_t>& main_bits,
                                   const vector<uint16_t>& exceptions) {
    vector<uint32_t> out;
    out.reserve(main_bits.size());
    for (size_t i = 0; i < main_bits.size(); ++i) {
        out.push_back(exceptions[i] ? exceptions[i] : main_bits[i]);
    }
    return out;
}

// ------------- Elias-Fano 分区 + SIMD ----------------

struct EFBlock {
    vector<uint32_t> low_bits;
    vector<uint8_t>  high_bits; // sparse bitmap
    int L;
};

EFBlock ef_encode(const vector<uint32_t>& block) {
    int n = block.size(); // 1. 序列长度 n
    uint32_t u = block.back(); // 2. 最大值 u —— 因为 block 已排好序，最后一个元素即最大值
    //𝑛：当前分区中元素的个数。
    // 𝑢 ：元素的最大值，用于推导高位位图的总长度。
    int L = max(1, (int)floor(log2((double)u / n)));

    /*
    为什么要算 L = floor(log₂(u/n))
    𝑛：你要编码的号码个数，这里是 4。
    𝑢：这串号码的最大值（因为有序，最后一个就是最大值），这里是 20。
    𝑢/𝑛 ：把 “最大值” 平均分给“每一个号码”，得到“平均间隔”≈ 20 / 4 = 5。
    log₂(5)：表示要用多少二进制位才能表示 “间隔 5” 这样大小的数字。
    log2(5)≈2.32
    log 2(5)≈2.32，意味着至少需要 3 位二进制才能不丢信息。但为了节省空间，我们取 下整（floor）：
    ⌊2.32⌋=2⌊2.32⌋=2。取下整后如果刚好是 2 位，你能表示的范围是 0…3（共 4 种可能），虽然不足以覆盖所有 0…4，但结合高位信息就足够了。
    */

    uint32_t mask = (1u << L) - 1;
    vector<uint32_t> low(n);
    vector<uint32_t> highs(n);
    for (int i = 0; i < n; ++i) {
        low[i] = block[i] & mask;
        highs[i] = block[i] >> L;
    }
    int size = highs.back() + n;
    vector<uint8_t> high_bits(size, 0);
    for (int i = 0; i < n; ++i) {
        high_bits[highs[i] + i] = 1;
    }
    return {low, high_bits, L};
}

vector<uint32_t> ef_decode_scalar(const EFBlock& blk) {
    vector<uint32_t> out;
    int n = blk.low_bits.size();
    out.reserve(n);
    int count = 0;
    for (int i = 0; i < (int)blk.high_bits.size() && count < n; ++i) {
        if (blk.high_bits[i]) {
            uint32_t high_val = i - count;
            out.push_back((high_val << blk.L) | blk.low_bits[count]);
            count++;
        }
    }
    return out;
}

// SIMD 解码：一次处理 16 位低位+高位计算
vector<uint32_t> ef_decode_simd(const EFBlock& blk) {
    int n = blk.low_bits.size();
    vector<uint32_t> out(n);
    // 1. 收集 high positions
    vector<uint32_t> positions;
    positions.reserve(n);
    for (int i = 0; i < (int)blk.high_bits.size(); ++i) {
        if (blk.high_bits[i]) positions.push_back(i);
    }
    // 2. 计算 high_vals = positions[k] - k
    //    用 AVX2 一次处理8个
    int k = 0;
    const int stride = 8;
    for (; k + stride <= n; k += stride) {
        // load positions
        __m256i pos = _mm256_loadu_si256((__m256i*)(positions.data() + k));
        // load indices [k..k+7]
        alignas(32) uint32_t idxs[stride];
        for (int i = 0; i < stride; ++i) idxs[i] = k + i;
        __m256i idx = _mm256_load_si256((__m256i*)idxs);
        // high_vals = pos - idx
        __m256i highs = _mm256_sub_epi32(pos, idx);
        // low_vals
        __m256i lows = _mm256_loadu_si256((__m256i*)(blk.low_bits.data() + k));
        // shift highs: highs << L
        __m256i shifted = _mm256_slli_epi32(highs, blk.L);
        // combine
        __m256i vals = _mm256_or_si256(shifted, lows);
        // store
        _mm256_storeu_si256((__m256i*)(out.data() + k), vals);
    }
    // 3. 处理剩余
    for (; k < n; ++k) {
        uint32_t high_val = positions[k] - k;
        out[k] = (high_val << blk.L) | blk.low_bits[k];
    }
    return out;
}

int main() {
    // 示例数据
    vector<uint32_t> doc_ids = {3, 10, 27, 28, 55, 120, 121, 300};
    // 1) 转 delta
    vector<uint32_t> deltas;
    deltas.push_back(doc_ids[0]);
    for (size_t i = 1; i < doc_ids.size(); ++i)
        deltas.push_back(doc_ids[i] - doc_ids[i-1]);

    // PForDelta 流程
    auto [mb, ex] = pfordelta_encode(deltas, 6);
    auto pfd_out = pfordelta_decode(mb, ex);
    cout << "PForDelta Decode:";
    for (auto v : pfd_out) cout << ' ' << v;
    cout << "\n";

    // PEF+SIMD 流程
    int B = 4;
    vector<EFBlock> blocks;
    for (size_t i = 0; i < doc_ids.size(); i += B) {
        vector<uint32_t> block(doc_ids.begin() + i,
                                doc_ids.begin() + min(doc_ids.size(), i + B));
        blocks.push_back(ef_encode(block));
    }
    vector<uint32_t> out_simd;
    for (auto& blk : blocks) {
        auto part = ef_decode_simd(blk);
        out_simd.insert(out_simd.end(), part.begin(), part.end());
    }
    cout << "PEF+SIMD Decode:";
    for (auto v : out_simd) cout << ' ' << v;
    cout << "\n";

    return 0;
}

/*
优化点说明：
1. PForDelta: 标量判断 & 单个整数处理
2. PEF+SIMD: EF分区后，高位bitmap更小；用AVX2一次处理8个整数的 high_val 计算和 low_bits 拼接
   - _mm256_loadu_si256/_mm256_storeu_si256: 向量内存读写
   - _mm256_sub_epi32: 并行减法
   - _mm256_slli_epi32: 并行左移
   - _mm256_or_si256: 并行或操作
*/

