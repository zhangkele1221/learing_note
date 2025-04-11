# 关于NUMA
NUMA(Non-Uniform Memory Access)是一种内存架构，它允许多个处理器访问不同的物理内存区域，从而提高并行计算的性能。numa的主要优势是减少了内存访问的延迟和带宽限制，因为每个处理器可以优先使用自己附近的内存。很多现代处理器（例如Intel和AMD的多核处理器）都支持NUMA架构。
在NUMA架构中，每个处理器都会独立连接到一部分内存（称为本地内存），各个CPU之间则是通过QPI(Quick Path Interconnect)总线进行连接，CPU可以通过该总线访问其他CPU所辖内存（成为远程内存）。在该架构下，访问远程内存的时延会明显高于访问本地内存。因此，在追求性能的场景下就需要认真考虑如何合理分配内存以适应NUMA架构。

# NUMA的配置
在Linux上，可以通过$ dmesg| grep -i numa命令检查系统是否支持NUMA。如果支持，可以通过$ sudo apt install numactl命令安装numactl工具。然后运行$ numactl --hardware来查看当前系统NUMA节点的状态。
此外，还可以通过numactl工具将程序绑定到指定的CPU核心或NUMA节点上。例如使用以下命令，将CPU0~15核心绑定到helloworld.out上:$ numactl -C 0-15 ./helloworld.out

# NUMA的一些问题（陷阱）
内存碎片化：由于不同的处理器可能会同时申请和释放内存，导致物理内存区域出现不连续的空闲块，这会降低内存利用率和分配效率。
内存迁移：如果一个进程在运行过程中被调度到另一个处理器上，它可能会发现自己的内存分布在不同的物理区域上，这会增加内存访问的延迟和开销。
内存亲和性：如果一个进程需要与其他进程或设备进行数据交换，它可能会希望将自己的内存分配在与之通信的对象相近的物理区域上，这样可以减少数据传输的时间和消耗。
同理，一个进程中的多个线程如果分布在不同的NUMA节点上，并且需要访问远程内存，那么它们就可能会遇到远程访存的瓶颈。
为了解决这些问题，我们需要对numa下的内存分配进行优化。一种常用的方法是使用numa-aware的内存分配器，它可以根据进程的运行情况和需求，动态地调整内存分配策略，从而提高内存性能和效率。

使用首次适应（first-fit）或最佳适应（best-fit）算法，尽量将内存分配在同一个物理区域上，避免内存碎片化。
使用页迁移（page migration）或页复制（page replication）技术，根据进程的位置和活跃度，将其内存迁移到或复制到合适的物理区域上，减少内存迁移。
使用亲和性掩码（affinity mask）或亲和性组（affinity group）机制，根据进程的通信模式和频率，将其内存分配在与之相关的对象相近的物理区域上，提高内存亲和性。

# NUMA-aware
NUMA-aware指能够感知并利用NUMA架构特性的软件或程序（需要预先使用命令$ sudo apt-get install libnuma-dev安装libnunma-dev库）。如下的代码示例中，首先获取了NUMA节点的数量,然后为每个NUMA节点创建一个线程。
每个线程中使用numa_run_on_node将线程绑定到指定的NUMA节点上，然后使用numa_alloc_local在本地内存中分配内存。由于各个线程被绑定到了不同的NUMA节点，线程均在各自节点上分配访问内存，程序性能大幅提升.


```cpp
#include<iostream>
#include<thread>
#include<vector>
#include<numa.h>

int main() {
    // 获取 NUMA 节点数量
    int num_nodes = numa_num_configured_nodes();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_nodes; i++) {
        threads.emplace_back([i] {
            // 绑定到指定 NUMA 节点
            numa_run_on_node(i);
            // 在本地 NUMA 节点分配内存
            int *data = (int *)numa_alloc_local(1024 * sizeof(int));

            // 处理数据
            for (int j = 0; j < 1024; j++) {
                data[j] = j;
            }

            // 释放内存
            numa_free(data, 1024 * sizeof(int));
        });
    }

    // 等待所有线程完成
    for (auto &t : threads) {
        t.join();
    }
    
    return 0;
}
```
# 补充
如果开启了NUMA，程序分配内存时就会优先分配本地内存，当本地内存空间不够时可以选择回收本地的PageCache（该策略一般会比较慢）或者是到其他NUMA上分配内存，可以通过Linux参数zone_reclaim_mode来配置，默认是到其他NUMA节点分配内存。





### **NUMA（非统一内存访问）学习指南**

---

#### **1. NUMA 是什么？**
- **全称**：Non-Uniform Memory Access（非统一内存访问）
- **背景**：为多处理器系统设计，解决传统SMP（对称多处理）架构的扩展性问题。在SMP中，所有CPU通过共享总线访问内存，随着CPU数量增加，总线争用导致性能下降。
- **核心思想**：将内存和CPU划分为多个节点（Node），每个节点内的CPU访问本地内存更快，访问其他节点的内存（远程内存）更慢。

---

#### **2. NUMA 的基本原理**
- **节点（Node）**：一组CPU核心 + 本地内存 + 互联控制器。
- **本地内存（Local Memory）**：节点内部的物理内存，访问延迟低。
- **远程内存（Remote Memory）**：其他节点的内存，访问延迟高（通常高1.5-3倍）。
- **互联总线**：节点间通过高速链路（如QPI、Infinity Fabric）连接。
---

#### **3. 为什么需要 NUMA？**
- **扩展性**：突破SMP的总线带宽瓶颈，支持更多CPU和内存。
- **性能优化**：减少远程内存访问，降低延迟。
- **能效**：本地内存访问更省电。

---

#### **4. NUMA 的组成**
- **NUMA节点**：CPU、内存、I/O的集合。
- **本地内存控制器**：管理节点内内存分配。
- **跨节点互联**：如Intel的QPI（QuickPath Interconnect）、AMD的Infinity Fabric。
- **缓存一致性协议**：保证多节点间缓存数据一致性（如CC-NUMA）。

---

#### **5. NUMA 的关键特性**
- **内存访问延迟差异**：本地 vs. 远程。
- **本地内存优先**：操作系统和应用程序需尽量使用本地内存。
- **缓存一致性**：确保跨节点数据同步（通过MOESI等协议）。
- **动态资源分配**：支持内存和CPU的热插拔。

---

#### **6. 应用场景**
- **高性能计算（HPC）**：如科学计算、气象模拟。
- **虚拟化**：虚拟机（VM）绑定到特定NUMA节点，减少跨节点访问。
- **数据库**：如MySQL、Oracle，通过NUMA优化减少锁竞争。
- **大数据处理**：如Apache Spark，任务调度考虑NUMA拓扑。

---

#### **7. Linux 中的 NUMA 管理**
- **查看NUMA信息**：
  ```bash
  lscpu | grep NUMA        # 查看NUMA节点数
  numactl --hardware       # 详细NUMA拓扑
  ```
- **numactl 工具**：控制进程的内存和CPU绑定。
  ```bash
  numactl --cpunodebind=0 --membind=0 ./program  # 绑定到节点0
  ```
- **内核参数**：
  ```bash
  vm.zone_reclaim_mode = 1     # 优先回收本地内存
  ```

---

#### **8. NUMA 优化策略**
- **内存分配策略**：
  - **默认（localalloc）**：优先本地内存。
  - **绑定（membind）**：强制指定内存节点。
  - **交错（interleave）**：轮询分配内存到多个节点（适用于均匀访问场景）。
- **CPU绑定**：避免进程跨节点调度（`taskset`或`numactl`）。
- **大页内存（HugePage）**：减少页表开销，提升TLB命中率。
- **监控工具**：
  - `numastat`：查看NUMA内存分配统计。
  - `numad`：自动优化NUMA资源分配。

---

#### **9. 挑战与注意事项**
- **编程复杂性**：需显式管理内存和线程绑定。
- **配置敏感**：错误配置可能导致性能下降（如频繁远程访问）。
- **内存碎片**：长期运行的系统可能出现本地内存不足。
- **异构系统**：GPU、FPGA等设备可能属于特定NUMA节点。

---

通过理解NUMA的原理并结合实际工具进行调优，可以显著提升多核系统的性能效率。建议从简单的绑定实验开始，逐步深入分析应用的内存访问模式。




