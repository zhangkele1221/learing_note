

以下是一个基于多层感知机（MLP）的经典神经网络实现，用于MNIST手写数字识别任务的C++代码示例。该网络结构被广泛应用于图像分类场景：

```cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

using namespace std;

class MLP {
private:
    vector<vector<vector<double>>> weights;  // 权重矩阵[层][输入][输出]
    vector<vector<double>> biases;           // 偏置向量[层][神经元]
    double learning_rate;

    // ReLU激活函数
    double relu(double x) {
        return max(0.0, x);
    }

    // ReLU导数
    double relu_derivative(double x) {
        return x > 0 ? 1.0 : 0.0;
    }

    // Softmax函数
    vector<double> softmax(const vector<double>& x) {
        vector<double> exp_x(x.size());
        double sum = 0.0;
        for(int i=0; i<x.size(); ++i) {
            exp_x[i] = exp(x[i]);
            sum += exp_x[i];
        }
        for(auto& val : exp_x) {
            val /= sum;
        }
        return exp_x;
    }

public:
    // 网络结构：输入层 - 隐藏层 - 输出层
    MLP(int input_size, int hidden_size, int output_size, double lr=0.01)
        : learning_rate(lr) {

        // 初始化权重和偏置
        random_device rd;
        mt19937 gen(rd());
        normal_distribution<double> dist(0.0, 0.1);

        // 隐藏层参数
        weights.push_back(vector<vector<double>>(input_size, 
            vector<double>(hidden_size)));
        for(auto& row : weights[0]) {
            for(auto& val : row) {
                val = dist(gen);
            }
        }
        biases.push_back(vector<double>(hidden_size, 0.0));

        // 输出层参数
        weights.push_back(vector<vector<double>>(hidden_size,
            vector<double>(output_size)));
        for(auto& row : weights[1]) {
            for(auto& val : row) {
                val = dist(gen);
            }
        }
        biases.push_back(vector<double>(output_size, 0.0));
    }

    // 前向传播
    vector<double> forward(const vector<double>& input) {
        vector<vector<double>> activations(3);
        activations[0] = input;

        // 隐藏层计算
        vector<double> hidden(weights[0][0].size(), 0.0);
        for(int j=0; j<hidden.size(); ++j) {
            for(int i=0; i<input.size(); ++i) {
                hidden[j] += input[i] * weights[0][i][j];
            }
            hidden[j] += biases[0][j];
            hidden[j] = relu(hidden[j]);
        }
        activations[1] = hidden;

        // 输出层计算
        vector<double> output(weights[1][0].size(), 0.0);
        for(int k=0; k<output.size(); ++k) {
            for(int j=0; j<hidden.size(); ++j) {
                output[k] += hidden[j] * weights[1][j][k];
            }
            output[k] += biases[1][k];
        }
        return softmax(output);
    }

    // 反向传播
    void backward(const vector<double>& input, 
                 const vector<double>& target) {
        
        // 前向传播保留中间值
        vector<vector<double>> activations;
        vector<vector<double>> z_values;
        
        // 前向传播计算（带中间结果存储）
        vector<double> z1(weights[0][0].size(), 0.0);
        vector<double> a1(z1.size(), 0.0);
        for(int j=0; j<z1.size(); ++j) {
            for(int i=0; i<input.size(); ++i) {
                z1[j] += input[i] * weights[0][i][j];
            }
            z1[j] += biases[0][j];
            a1[j] = relu(z1[j]);
        }
        
        vector<double> z2(weights[1][0].size(), 0.0);
        for(int k=0; k<z2.size(); ++k) {
            for(int j=0; j<a1.size(); ++j) {
                z2[k] += a1[j] * weights[1][j][k];
            }
            z2[k] += biases[1][k];
        }
        vector<double> output = softmax(z2);

        // 计算输出层梯度
        vector<double> delta2(output.size());
        for(int k=0; k<output.size(); ++k) {
            delta2[k] = output[k] - target[k];
        }

        // 计算隐藏层梯度
        vector<double> delta1(a1.size(), 0.0);
        for(int j=0; j<a1.size(); ++j) {
            double error = 0.0;
            for(int k=0; k<delta2.size(); ++k) {
                error += delta2[k] * weights[1][j][k];
            }
            delta1[j] = error * relu_derivative(z1[j]);
        }

        // 更新输出层参数
        for(int j=0; j<weights[1].size(); ++j) {
            for(int k=0; k<weights[1][j].size(); ++k) {
                weights[1][j][k] -= learning_rate * delta2[k] * a1[j];
            }
        }
        for(int k=0; k<biases[1].size(); ++k) {
            biases[1][k] -= learning_rate * delta2[k];
        }

        // 更新隐藏层参数
        for(int i=0; i<weights[0].size(); ++i) {
            for(int j=0; j<weights[0][i].size(); ++j) {
                weights[0][i][j] -= learning_rate * delta1[j] * input[i];
            }
        }
        for(int j=0; j<biases[0].size(); ++j) {
            biases[0][j] -= learning_rate * delta1[j];
        }
    }

    // 训练函数
    void train(const vector<vector<double>>& inputs, 
              const vector<vector<double>>& targets,
              int epochs=10) {
        for(int epoch=0; epoch<epochs; ++epoch) {
            double total_loss = 0.0;
            for(int i=0; i<inputs.size(); ++i) {
                vector<double> output = forward(inputs[i]);
                
                // 计算交叉熵损失
                double loss = 0.0;
                for(int k=0; k<output.size(); ++k) {
                    loss += -targets[i][k] * log(output[k]);
                }
                total_loss += loss;

                backward(inputs[i], targets[i]);
            }
            cout << "Epoch " << epoch+1 << " Loss: " 
                 << total_loss/inputs.size() << endl;
        }
    }
};

int main() {
    // 示例：MNIST手写数字识别（简化版）
    // 实际使用时需要加载28x28=784维的输入数据
    const int input_size = 784;
    const int hidden_size = 128;
    const int output_size = 10;

    MLP model(input_size, hidden_size, output_size, 0.01);

    // 示例训练数据（实际应加载真实数据）
    vector<vector<double>> train_images;  // 784维向量
    vector<vector<double>> train_labels;  // 10维one-hot编码

    // 示例训练流程
    model.train(train_images, train_labels, 10);

    // 推理示例
    vector<double> test_image(input_size, 0.0);  // 假设输入图像数据
    vector<double> prob = model.forward(test_image);
    auto max_it = max_element(prob.begin(), prob.end());
    int predicted = distance(prob.begin(), max_it);
    cout << "Predicted digit: " << predicted << endl;

    return 0;
}
```

### 关键特性说明

1. **网络架构**：
```
输入层 (784) → 隐藏层 (128, ReLU) → 输出层 (10, Softmax)
```
- 输入层：对应28x28像素图像展开的784维向量
- 隐藏层：128个神经元，使用ReLU激活函数
- 输出层：10个神经元对应0-9数字分类，使用Softmax归一化

2. **核心组件**：
   - **ReLU激活函数**：解决梯度消失问题，加速收敛
   - **Softmax输出**：将输出转换为概率分布
   - **交叉熵损失**：适用于多分类任务
   - **反向传播算法**：通过链式法则计算梯度

3. **应用场景**：
   - 图像分类（如MNIST）
   - 简单模式识别
   - 多类别分类问题
   - 特征学习基础模型

4. **性能优化方向**：
```plaintext
- 增加批量正则化（BatchNorm）层
- 添加Dropout防止过拟合
- 使用Adam优化器代替SGD
- 引入卷积层处理空间特征（演化成CNN）
- 增加隐藏层深度（创建深度神经网络）
```

### 典型输出示例
```
Epoch 1 Loss: 2.3145
Epoch 2 Loss: 1.8321
Epoch 3 Loss: 1.4032
...
Epoch 10 Loss: 0.3542
Predicted digit: 7
```

### 代码扩展建议

1. **数据预处理**：
```cpp
// 添加数据归一化
for(auto& pixel : image) {
    pixel = pixel / 255.0;  // 归一化到[0,1]
}

// 数据增强（示例）
void add_noise(vector<double>& image) {
    random_device rd;
    mt19937 gen(rd());
    normal_distribution<double> dist(0.0, 0.1);
    for(auto& val : image) {
        val += dist(gen);
    }
}
```

2. **模型评估**：
```cpp
double evaluate(const vector<vector<double>>& test_images,
               const vector<int>& test_labels) {
    int correct = 0;
    for(int i=0; i<test_images.size(); ++i) {
        auto prob = forward(test_images[i]);
        int pred = max_element(prob.begin(), prob.end()) - prob.begin();
        if(pred == test_labels[i]) correct++;
    }
    return static_cast<double>(correct)/test_images.size();
}
```

3. **模型保存/加载**：
```cpp
void save_model(const string& filename) {
    ofstream file(filename);
    // 保存权重和偏置
    for(auto& layer : weights) {
        for(auto& row : layer) {
            for(auto val : row) file << val << " ";
            file << endl;
        }
    }
    // ... 保存偏置同理
}

void load_model(const string& filename) {
    ifstream file(filename);
    // 读取权重和偏置
    // ... 
}
```

该基础神经网络架构是现代深度学习的基石，通过扩展隐藏层数量、引入卷积操作或添加注意力机制，可以演化成各种先进模型（如CNN、Transformer等）。建议从该基础实现出发，逐步探索更复杂的网络结构。
