#include "pch.h"
#include "myallocator.h"
#include <vector>
using namespace std;

int main()
{
	vector<int, myallocator<int>> vec;

	for (int i = 0; i < 100; ++i)
	{
		int data = rand() % 1000;
		vec.push_back(data);
	}

	for (int val : vec)
	{
		cout << val << " ";
	}
	cout<<endl;

	return 0;
}





class Foo {
    mutex mtx_1, mtx_2;
    unique_lock<mutex> lock_1, lock_2;
public:
    Foo() : lock_1(mtx_1, try_to_lock), lock_2(mtx_2, try_to_lock) {
    }

    void first(function<void()> printFirst) {
        printFirst();
        lock_1.unlock();
    }

    void second(function<void()> printSecond) {
        lock_guard<mutex> guard(mtx_1);
        printSecond();
        lock_2.unlock();
    }

    void third(function<void()> printThird) {
        lock_guard<mutex> guard(mtx_2);
        printThird();
    }
};



//多线程打印.....  使用 unix 最简单的写法 信号量........
#include <semaphore.h>

#include <iostream>
#include <functional>
using namespace std;

class Foo {
private:
    sem_t sem_1, sem_2;//定义两个信号量  sem_t是类型......

public:
    Foo() {
        sem_init(&sem_1, 0, 0), sem_init(&sem_2, 0, 0);//构造函数中.......sem_init (信号量变量，0 ,0);
    }

    void first(function<void()> printFirst) {//......function类型入参数....
        printFirst();//......调用入参 function....
        sem_post(&sem_1);//..激发下一 等待 在等待的 函数.....调用中的信号量.....
    }

    void second(function<void()> printSecond) {
        sem_wait(&sem_1);//...显然在等上一个信号操作完成 sem_wait......
        printSecond();
        sem_post(&sem_2);//......激发下一个函数中调用信号量的......
    }

    void third(function<void()> printThird) {
        sem_wait(&sem_2);//......等待被激发.......
        printThird();
    }
};

void printFirst(){
    cout<<"printFirst"<<endl;
}
void printSecond(){
    cout<<"printSecond"<<endl;
}
void printThird(){
    cout<<"printThird"<<endl;
}

int main() {
	Foo foo;
	int n = 10;
	while(n--) {
	    foo.first(printFirst);
	    foo.first(printSecond);
	    foo.first(printThird);
	}
	return 0;
}


// lru
#include<map>
#include<list>
struct Node
{
    Node(int k = 0, int v = 0) : key(k), val(v) {}
    int key;
    int val;
};

class Solution {
private:
    list<Node *> L;//这个成员设计的要合理  list<node*> l;  借助 STL 重的list 来实现链表就不需要自己实现链表了.....
    map<int, list<Node *>::iterator> H;//这个map 相当于 “索引了” list中所有 node* 了 
    int cap;//表示list 大小就是lru的大小
public:
    /**
     * lru design
     * @param operators int整型vector<vector<>> the ops
     * @param k int整型 the k
     * @return int整型vector
     */
    vector<int> LRU(vector<vector<int> >& operators, int k) {
        cap = k;
        vector<int> res;
        for (auto &item : operators) {     //  [ [1,1,1]  ,  [2,1]  , [1,2,3] ]
            if (item[0] == 1) {//这个是牛客上暴露的接口特殊需要
                set(item[1], item[2]);//着就是 按照牛科的特殊需求 搞的一个遍历 根据数组第一位数 1代表set 2代表get
            } else {
                res.push_back(get(item[1]));
            }
        }
        return res;
    }
    
    int del(list<Node *>::iterator& it) {
        L.erase(it);//list 删除 node*
        H.erase((*it)->key);//map 删除 元素
        int val = (*it)->val;
        delete *it;// 释放 node*
        return val;
    }
    
    void add(int x, int y) {
        Node * a = new Node(x, y);//构建新的 node*
        L.push_front(a);// 将新建立的 node* 存入链表头
        H[x] = L.begin();//再将链表头 存放的map  一个是key 一个是俩表头的 node*
        if (L.size() > cap) {//add 到 list 后面超过容量了 就删掉list 最后一个node
            auto last = L.end();
            -- last;
            del(last);
        }
    }
    
    void set(int x, int y) {
        auto it = H.find(x);//根据key 去map 中查找 list<Node *>::iterator 迭代器 // map<int, list<Node *>::iterator> H;
        if (it != H.end()) {
            del(it->second);//如果这个key 原来就存在先删掉  这个map重second 迭代器....
        }
        add(x, y);//
    }
    
    int get(int x) {
        auto it = H.find(x);
        if (it != H.end()) {
            int val = del(it->second);//存在 先删除  返回 val
            add(x, val);//这里又进行一次add 就是将其放入 list 头部 ...
            return val;
        }
        return -1;// key 不存在 返回 -1  
    }
};


//lru.....实现.....
struct DLinkedNode {
    int key, value;
    DLinkedNode* prev;
    DLinkedNode* next;
    DLinkedNode(): key(0), value(0), prev(nullptr), next(nullptr) {}
    DLinkedNode(int _key, int _value): key(_key), value(_value), prev(nullptr), next(nullptr) {}
};

class LRUCache {
private:
    unordered_map<int, DLinkedNode*> cache;//
    DLinkedNode* head;//头
    DLinkedNode* tail;//尾
    int size;
    int capacity;

public:
    LRUCache(int _capacity): capacity(_capacity), size(0) {
        // 使用伪头部和伪尾部节点
        head = new DLinkedNode();
        tail = new DLinkedNode();
        head->next = tail;
        tail->prev = head;
    }
    
    int get(int key) {
        if (!cache.count(key)) {
            return -1;
        }
        // 如果 key 存在，先通过哈希表定位，再移到头部
        DLinkedNode* node = cache[key];
        moveToHead(node);
        return node->value;
    }
    
    void put(int key, int value) {
        if (!cache.count(key)) {
            // 如果 key 不存在，创建一个新的节点
            DLinkedNode* node = new DLinkedNode(key, value);
            // 添加进哈希表
            cache[key] = node;
            // 添加至双向链表的头部
            addToHead(node);
            ++size;
            if (size > capacity) {
                // 如果超出容量，删除双向链表的尾部节点
                DLinkedNode* removed = removeTail();//.............删除尾巴......
                // 删除哈希表中对应的项
                cache.erase(removed->key);
                // 防止内存泄漏
                delete removed;
                --size;
            }
        }
        else {
            // 如果 key 存在，先通过哈希表定位，再修改 value，并移到头部
            DLinkedNode* node = cache[key];
            node->value = value;
            moveToHead(node);
        }
    }

    void addToHead(DLinkedNode* node) {// addToHead 四部曲 因为是双向链表嘛  又是插入  肯定要有四个指针的变更
        node->prev = head;//放到头部 也要这样操作  head 是伪节点 所以 现在的node的prev 当然是head了
        node->next = head->next;//新的头部节点的下一个节点肯定是  原来伪节点的 next 节点啦...
        head->next->prev = node;//下一个节点的  pre 肯定是 新的 node 啦
        head->next = node;//
    }
    
    void removeNode(DLinkedNode* node) {//remove 删除肯定只涉及到两个指针啦..
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void moveToHead(DLinkedNode* node) {//分两步走
        removeNode(node);//断开来
        addToHead(node);//添加到头..
    }

    DLinkedNode* removeTail() {
        DLinkedNode* node = tail->prev;
        removeNode(node);
        return node;
    }
};


//为什么要用 假节点的head 自己想啊.......
ListNode* removeElements(ListNode* head, int val) {
    ListNode* head_head = new ListNode(1);
    head_head->next = head;
    ListNode*cur = head_head;
    while(cur->next!=nullptr){
        if(cur->next->val == val){
            ListNode* temp = cur->next->next;
            delete cur->next;
            cur->next = temp;
        }else{
            cur = cur->next;
        }
    }
    head = head_head->next;
    delete head_head;
    return head;
}

//链表反转
//.....反转链表........ 双指针法.... 其实很简单 真的很简单......
ListNode* reverseList(ListNode* head) {
    ListNode* temp; // 保存cur的下一个节点     .......记住这句话.....零时节点这就是用来保存当前节点的下一个节点。
    ListNode* cur = head;
    ListNode* pre = NULL;//......这个就是用来返回最后的头节点的......
    while(cur) {
        temp = cur->next;  // 保存一下 cur的下一个节点，因为接下来要改变cur->next  .......
        cur->next = pre; // 翻转操作.........  未来当前节点的下一个节点.....
        // 更新pre 和 cur指针
        pre = cur;
        cur = temp;
    }
    return pre;
}

//单链表排序 
ListNode* sortInList(ListNode* head) {
    // write code here
    vector<int> temp;
    ListNode*cur = head;
    while(cur){
        temp.push_back(cur->val);
        cur=cur->next;
    }
    sort(temp.begin(), temp.end());
    int i = 0;
    cur = head;
    while(cur){
        cur->val=temp[i];
        cur=cur->next;
        i++;
    }
    return head;
}



 83. 删除排序链表中的重复元素  
 !!!!这个比较简单相当于去重复  由于我们删除的思路中肯定不会删除头部节点 所以无需构造假head 进行遍历删除.... 
 保留一个... 一次性要写出来.......


 //82 删除排序链表中的重复元素 II  这里就是将所有重复的都干掉......  !!! 这里就要考虑到 头部节点要删除了....设一个假的head....
 ListNode* deleteDuplicates(ListNode* head) {
    if(head==nullptr) return head;
    ListNode*dump = new ListNode(0);
    dump->next = head;
    ListNode*cur = dump;//这里为什么创建前驱节点想象就明白了啊...
    while(cur->next&&cur->next->next){//cur->next 这个是原始的链表的第一个节点.  cur->next->next 这个是原始链表的第二个节点
        if(cur->next->val==cur->next->next->val){
            int x = cur->next->val;
            while(cur->next&&cur->next->val==x){
                cur->next = cur->next->next;// 循环删除后续重复的....
            }
            
        }else{
            cur = cur->next;//删除自己哦 这里... 因为这一题就是干掉所有重复的
        }
    }
    return dump->next;
}


//删除 倒数的第k个节点...
ListNode* removeNthFromEnd(ListNode* head, int n) {
    ListNode* head_head = new ListNode(1);//.....构造假的头节点 是删除节点过程中 遇到头节点要删除的情况方便作出的一种操作....
    head_head->next = head;
    ListNode* fast = head_head; 
    ListNode* solw = head_head;
    for(int i =0;i<n;i++) {
        fast = fast->next;
    }
    while(fast->next) {
        fast = fast->next;
        solw = solw->next;
    }
    solw->next = solw->next->next;
    head = head_head->next;
    delete head_head;
    return head;
}
// 链表节点两两交换.......  如... 1234   2143
ListNode* swapPairs(ListNode* head) {
    ListNode* temp = new ListNode(0);
    temp->next = head;
    ListNode*cur = temp;
    while(cur->next!=nullptr &&cur->next->next!=nullptr ) {
        ListNode* tem = cur->next;
        ListNode* tem1 = cur->next->next->next;

        cur->next = cur->next->next;   //步骤一
        cur->next->next = tem;         //步骤二
        cur->next->next->next = tem1;   // 步骤三

        cur = cur->next->next; // cur移动两位，准备下一轮交换
    }
    return temp->next;
}

//找出并返回两个单链表相交的  起始节点。如果两个链表没有交点，返回 null 。
ListNode *getIntersectionNode(ListNode *headA, ListNode *headB) {//找到 并且返回的是  起始节点..... 
    ListNode* cur1=headA;
    ListNode* cur2=headB;
    int len1 =0;
    int len2 =0;
    while(cur1){
        len1++;
        cur1 = cur1->next;
    }
    while(cur2){
        len2++;
        cur2 = cur2->next;
    }
    cur1=headA;
    cur2=headB;
    if(len1>len2){
        int diff = len1 - len2;
        while(diff--){
            cur1 = cur1->next;
        }
    }else{
        int diff = len2 - len1;
        while(diff--){
            cur2 = cur2->next;
        }
    }
    while(cur1){
        if(cur2 == cur1)
            return cur1;
        cur1 = cur1->next;
        cur2 = cur2->next;
    }
    return NULL;

}



//有环判断......
//这个方法很好.... 还没理解......
bool hasCycle(ListNode *head) {
    ListNode *pre =NULL;
    ListNode *pnode = head;
    while(pnode!=NULL)
    {
        //if(pnode==head)return true;
        if(pnode->next==pnode)return true;

        pre = pnode;
        pnode = pre->next;
        pre->next = pre;
    }
    return false;
    
}


//快慢指针.................

bool hasCycle(ListNode *head) {
    if(head==nullptr||head->next==nullptr)return false;//这里直接说命令 一个node 肯定不会形成环状的.....所以 head->next判断了.....

    ListNode*fast = head, *slow = head;

    while(fast != nullptr) {
        if(fast->next == nullptr)return false;//有环 就不允许有 nullptr
        fast = fast->next;//上一步是先判断  这里再前进.......
        if(fast->next == nullptr)return false;//同样道理...... 这里有个特点就是 先判断后移动 
        fast = fast->next;//同上......

        slow = slow->next; //移动为什么不能和下面的判断 掉到次序呢? 为了保持一步之差啊 颠倒了 就可能不是一步之差了可能是两步了....
        if(fast == slow)return true;
    }

    return false;
}



//两个堆栈实现队列...........
class Solution
{
public:
    void push(int node) {//stack1 只负责插入,缓存数据  stack2变空时,  清空stack1缓存并继续压入stack2
        stack1.push(node);
    }

    int pop() {
        if(stack2.empty()){//// stack2负责弹出最先压入的元素
            while(!stack1.empty()){
                stack2.push(stack1.top());
                stack1.pop();
            }
        }
        int tmp = stack2.top();
        stack2.pop();
        return tmp;
    }

private:
    stack<int> stack1;
    stack<int> stack2;
};


//层序遍历.....  队列先进先出..... 区分  使用stack 方式的 三种遍历方式.....!!!!!!!!!!!!
vector<int> PrintFromTopToBottom(TreeNode* root) {

    vector<int> vec;
    if(root ==NULL ) return vec ;
     
    queue<TreeNode*> q;//层序遍历借助的是 队列 先进先出  属于广度遍历  和迭代方法那种遍历借助 stack 思想肯是不一样的注意细节....!!!!!
     
    q.push(root);
     
    while(q.size()){

        TreeNode* pNode = q.front();
        q.pop();
         
        vec.push_back(pNode->val);
         
        if(pNode->left) q.push(pNode->left);

        if(pNode->right) q.push(pNode->right);

    }
    return vec;
}

//层序遍历.....
vector<vector<int> > levelOrder(TreeNode* root) {//看到没有只要将上面的改造下 加上一个行size 控制就OK了............

    vector<vector<int> > res;
    if(root == NULL)    return res;

    queue<TreeNode*> q;
    q.push_back(root);

    while(!q.empty())
    {
        vector<int> level;              //存放每一层的元素值
        int count = q.size();           //队列大小表示当前层数的元素个数......................
        while(count--)                  //count--逐个对该层元素进行处理
        {               
            TreeNode *temp = q.front();
            q.pop();
            level.push_back(temp->val);
            if(temp->left)   q.push_back(temp->left);
            if(temp->right)  q.push_back(temp->right);
        }
        res.push_back(level);           //将当层元素加入res中
    }
    return res;
}

//二叉树的最大深度.....
方法一: 递归
int maxDepth(TreeNode* root) {
    if (root == nullptr) return 0;
    return max(maxDepth(root->left), maxDepth(root->right)) + 1;
}

方法二: 广度优先遍历  和上面层序遍历有点类似..... 可以一起借鉴.......写.....
int maxDepth(TreeNode* root) {
    if (root == nullptr) return 0;
    queue<TreeNode*> q;
    q.push(root);
    int ans = 0;
    while (!q.empty()) {
        int sz = q.size();
        while (sz-- > 0) {//....... 这一层油多少个个节点.....
            TreeNode* node = q.front();
            q.pop();
            if (node->left) q.push(node->left);
            if (node->right) q.push(node->right);
        }
        ans += 1;
    } 
    return ans;
}


// 前序列  中序列   构建二叉树  构建右视图    前序列  后序 构建二叉树.......
方法一 递归......

[ 根节点, [左子树的前序遍历结果], [右子树的前序遍历结果] ]

[ [左子树的中序遍历结果], 根节点, [右子树的中序遍历结果] ]    //这两个图一定要画出来 帮助理解写代码...............

只要我们在中序遍历中定位到根节点，那么我们就可以分别知道左子树和右子树中的节点数目.........
由于同一颗子树的前序遍历和中序遍历的长度显然是相同的，因此我们就可以对应到前序遍历的结果中，对上述形式中的所有左右括号进行定位。

这样以来，我们就知道了左子树的前序遍历和中序遍历结果，以及右子树的前序遍历和中序遍历结果，
我们就可以递归地对构造出左子树和右子树，再将这两颗子树接到根节点的左右位置。

在中序遍历中对根节点进行定位时，一种简单的方法是直接扫描整个中序遍历的结果并找出根节点，--------------------但这样做的时间复杂度较高。

我们可以考虑使用哈希表来帮助我们快速地定位根节点。对于哈希映射中的每个键值对，键表示一个元素（节点的值),值表示其在中序遍历中的出现位置。
在构造二叉树的过程之前，我们可以对中序遍历的列表进行一遍扫描，就可以构造出这个哈希映射。
在此后构造二叉树的过程中，我们就只需要 O(1)O(1) 的时间对根节点进行定位了。


class Solution {
private:
    unordered_map<int, int> index;//这里为什么要存储 中序的下标和中序元素的索引关系 为什么是用中序的 自己想。中序的容易分割出 左右子树....

public:
    TreeNode* myBuildTree(const vector<int>& preorder, const vector<int>& inorder, 
    	                  int preorder_left, int preorder_right, int inorder_left, int inorder_right) {

        if (preorder_left > preorder_right) {
            return nullptr;
        }
        
        // 前序遍历中的第一个节点就是根节点
        int preorder_root = preorder_left;
        // 在中序遍历中定位根节点
        int inorder_root = index[preorder[preorder_root]];
        
        // 先把根节点建立出来
        TreeNode* root = new TreeNode(preorder[preorder_root]);
        // 得到左子树中的节点数目
        int size_left_subtree = inorder_root - inorder_left;
        //---------- 递归地构造左子树，并连接到根节点
        // 先序遍历中「从 左边界+1 开始的 size_left_subtree」个元素就对应了中序遍历中「从 左边界 开始到 根节点定位-1」的元素
        root->left = myBuildTree(preorder, inorder, preorder_left + 1, preorder_left + size_left_subtree, 
                                                         inorder_left, inorder_root - 1);
        //---------- 递归地构造右子树，并连接到根节点
        // 先序遍历中「从 左边界+1+左子树节点数目 开始到 右边界」的元素就对应了中序遍历中「从 根节点定位+1 到 右边界」的元素
        root->right = myBuildTree(preorder, inorder, preorder_left + size_left_subtree + 1, preorder_right, 
                                                                          inorder_root + 1, inorder_right);
        
        return root;
    }

    TreeNode* buildTree(vector<int>& preorder, vector<int>& inorder) {
        int n = preorder.size();
        // 构造哈希映射，帮助我们快速定位根节点
        for (int i = 0; i < n; ++i) {
            index[inorder[i]] = i;//......这里存储的是 key -- 和序列的下标........理解为什么要用中序的原因---要想明白
        }
        // 这里的入参数 变量 很重要要理解透彻了.......
        return myBuildTree(preorder, inorder, 0, n - 1, 0, n - 1);//入参都是 前序子序列 左边起点 和 右边界  中序 左边起点 和右边界
    }
};

class Solution {
private:
    unordered_map<int, int> index;

public:
    TreeNode* myBuildTree(const vector<int>& preorder, int preorder_left, int preorder_right,
                          const vector<int>& inorder, int inorder_left, int inorder_right) {

        if (preorder_left > preorder_right) {//.......递归结束条件......
            return nullptr;
        }
        
        // 前序遍历中的第一个节点就是根节点
        int preorder_root = preorder_left;
        // 在中序遍历中定位根节点
        int inorder_root = index[preorder[preorder_root]];
        
        // 先把根节点建立出来
        TreeNode* root = new TreeNode(preorder[preorder_root]);
        // 得到左子树中的节点数目
        int size_left_subtree = inorder_root - inorder_left;//.......这么多个节点........
        // 递归地构造左子树，并连接到根节点
        // 先序遍历中「从 左边界+1 开始的 size_left_subtree」个元素就对应了中序遍历中「从 左边界 开始到 根节点定位-1」的元素
        root->left = myBuildTree(preorder, preorder_left + 1, preorder_left + size_left_subtree, //........这里为什么直接是加上...preorder_left 已经向前走了一步.....
                                  inorder, inorder_left, inorder_root - 1);
        // 递归地构造右子树，并连接到根节点
        // 先序遍历中「从 左边界+1+左子树节点数目 开始到 右边界」的元素就对应了中序遍历中「从 根节点定位+1 到 右边界」的元素
        root->right = myBuildTree(preorder, preorder_left + size_left_subtree + 1, preorder_right, 
                                  inorder, inorder_root + 1, inorder_right);
        return root;
    }

    TreeNode* buildTree(vector<int>& preorder, vector<int>& inorder) {
        int n = preorder.size();
        // 构造哈希映射，帮助我们快速定位根节点
        for (int i = 0; i < n; ++i) {
            index[inorder[i]] = i; //............哈希表存放中序元素进行左右子树划分..........
        }
        return myBuildTree(preorder, 0, n - 1, inorder, 0, n - 1);
    }
};

   
    




方法二：迭代 .......






二叉树的右视图  

思路就是存储每一层的最右边的一个节点........  类似按层序遍历  或者 层序遍历返回 二维向量 思想差不多... 需要一口气写出来 bugfree 

 vector<int> rightSideView(TreeNode* root) {
 	//会层序遍历   返回二维向量 那种思想 存储每一层的最后一个 就OK了 题目easy!!



学会二叉树的层序遍历，可以一口气撸完leetcode上八道题目：...........牛逼总结........

102.二叉树的层序遍历
107.二叉树的层次遍历II  === 自底向上的层序遍历 简单吧..........
199.二叉树的右视图


637.二叉树的层平均值
429.N叉树的前序遍历
515.在每个树行中找最大值
116.填充每个节点的下一个右侧节点指针
117.填充每个节点的下一个右侧节点指针II



 }








// .......最长不重复子串........  最长无重复子数组 两题一样哈.....
int lengthOfLongestSubstring(string s) {
    if (!s.size()) return 0; // 判空
    int maxLength = 0; // 记录最长不包含重复字符的子字符串的长度
    int left = 0, right = 0; // 初始化双指针
    unordered_set<char> subStr; // 哈希表存储不包含重复字符的子串
    /*双指针遍历原字符串:
    * 外层左指针、内层右指针
    */
    while (left < s.size()) {
        /* 右指针不断右移遍历字符串
        *  当遇到的字符在哈希表中不存在时，将其插入哈希表
        *  遇到哈希表中存在的字符 或 到原字符串结尾时跳出循环
        */
        while (right < s.size() && !subStr.count(s[right])) {
            subStr.insert(s[right]);
            right++;
        }
        /* 更新最大子串的长度
        *  因为上面的循环跳出时right指向的是子串后面一个位置
        *  所以是right-left而不是right-left+1
        */
        maxLength = max(maxLength, right - left);
        /* 若上面循环跳出是因为右指针到原字符串末尾，
        *  说明遍历完毕，最长不包含重复字符的子串已经找到
        *  直接跳出循环
        */
        if (right == s.size()) break;
        /* 若上面循环跳出是因为遇到了哈希表中已有的字符
        *  则从左边开始挨个删除哈希表中的字符
        *  并且左指针右移
        */
        subStr.erase(s[left]);//........这里会一直删除到 subStr.count(s[right] 这里查不到为止.......
        left++;
    }
    return maxLength;
}


// 最长不重复 子串 滑动窗口...........
int lengthOfLongestSubstring(string s){

    if(s.size == 0){return 0;}

    int left =0, right = 0,maxLength =0;
    unordered_set<char> submap;

    while(left<s.size()){

      while(right<s.size()&& !submap.count(s[right])){
        submap.insert(s[right]);
        right++;
      }

      maxLength = max(maxLength ,right-left);

      if(right == s.size())break;

      submap.erase(s[left]);
      left++;
    }
    return maxLength;
}

//两个数之和  暴力法则......
vector<int> twoSum(vector<int>& nums, int target) {
        int n = nums.size();
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (nums[i] + nums[j] == target) {
                    return {i, j};
                }
            }
        }
        return {};
}

//两数之和  优化了的时间复杂度.... 空间复杂度为n了
vector<int> twoSum(vector<int>& nums, int target) {
  unordered_map<int, int> hashtable;
  for (int i = 0; i < nums.size(); ++i) {
      auto it = hashtable.find(target - nums[i]);
      if (it != hashtable.end()) {
          return {it->second, i};//...这里就是之前存的数据有结果了 匹配上了所以返回了结果.......
      }
      hashtable[nums[i]] = i;//这里就是存放之前 遍历过的数据 存数据和对应的标记.......
  }
  return {};
}


//在二叉树中找到两个节点的最近公共祖先  入参不同思想不同....
    unordered_map<int, int> parent;// 这个变量就是村当前节点父节点的值.....
    unordered_set<int> visited;
    void dfsfinParent(TreeNode* root){
        if(root ->left){
           parent[root->left->val] = root->val;//从上到下遍历 存储所有的父子节点的关系 key是子节点 val是父节点
            dfsfinParent(root->left);
        }
        if(root ->right){
            parent[ root->right->val] =root->val;
            dfsfinParent(root->right);
        }
         
    }
    int lowestCommonAncestor(TreeNode* root, int o1, int o2) {
        // write code here
        parent[root->val] = -1;// 设根节点的父节点为-1
        dfsfinParent(root);
        while(o1!= -1){
            visited.insert(o1);//这里存放的都是  向上遍历返回路径的节点 
            o1 = parent[o1];
        }
        while(o2 != -1){
            if(visited.count(o2)) return o2;//找到 了则返回 公共祖节点
            o2 = parent[o2];
        }
        return -1;
    }


// 从底部.....输出返回从地步往上...层结构......xxxxxx???
vector<vector<int>> levelOrderBottom(TreeNode* root) {
        auto levelOrder = vector<vector<int>>();
        if (!root) {
            return levelOrder;
        }
        queue<TreeNode*> q;
        q.push(root);
        while (!q.empty()) {
            auto level = vector<int>();
            int size = q.size();
            for (int i = 0; i < size; ++i) {
                auto node = q.front();
                q.pop();
                level.push_back(node->val);
                if (node->left) {
                    q.push(node->left);
                }
                if (node->right) {
                    q.push(node->right);
                }
            }
            levelOrder.push_back(level);
        }
        reverse(levelOrder.begin(), levelOrder.end());
        return levelOrder;
}


// 字符串 大数加法........
class Solution {
public:
    /**
     * 代码中的类名、方法名、参数名已经指定，请勿修改，直接返回方法规定的值即可
     * 计算两个数之和
     * @param s string字符串 表示第一个整数
     * @param t string字符串 表示第二个整数
     * @return string字符串
     */
    string solve(string s, string t) {
        // write code here
        int l = s.length();
        int r = t.length();
        if(!l) return t;
        if(!r) return s;
        int carry = 0,tmp = 0;
        if(l<r){
            for(int n=r-l;n>0;n--)//.....这里是干什么
                s = '0'+s;
            l=r;
        }else if(l>r){
            for(int n=l-r;n>0;n--)//.....这里是干什么
                t = '0'+t;
            l=r;
        }
        for(int i=s.size()-1;i>=0;i--){
            tmp = s[i]-'0'+t[i]-'0'+carry;
            if(tmp>=10){
                carry = 1;
                tmp -= 10;
            }else
                carry = 0;
            s[i] = tmp+'0';
        }

        if(carry)
            s = '1'+s;
        return s;
            
        
    }
};

//大数即字符串相互加减.........  有难度的.......
void add(string &ans,char x,char y,int &d)
{
    int res = x - '0' + y - '0' + d;
    ans += (res % 10 + '0');//保留位...
    d = res / 10;//..进位
}
 
string solve(string s, string t) {
    reverse(s.begin(),s.end());
    reverse(t.begin(),t.end());
    string ans = "";
    int d = 0;
    //前半截...
    if(s.length() < t.length())    swap(s,t);//.....方便循环用短的字符串为循环次数.....  s现在是个长串了....
    for(int i=0;i<t.length();i++)    add(ans,s[i],t[i],d);
    //后半截....
    for(int i = t.length();i<s.length();i++)    add(ans,s[i],'0',d);//计算剩下多出来的位置...........
    if(d)  ans += '1';//最后 如果还有进位需要加上.....

    reverse(ans.begin(),ans.end());
    return ans;
}

//合并有序链表.......
ListNode* Merge(ListNode* pHead1, ListNode* pHead2) {
    if(pHead1== nullptr)  // 其中任何一个head为空 就返回另一个head
        return pHead2;
    if(pHead2 == nullptr)
        return pHead1;
    
    ListNode* pMergeHead = nullptr;//定义一个合并后的链表新...head...节点.....
    
    if(pHead1->val<pHead2->val){
        pMergeHead = pHead1;
        pMergeHead->next = Merge(pHead1->next, pHead2);
    }else{
        pMergeHead = pHead2;
        pMergeHead->next = Merge(pHead1, pHead2->next);
    }
    return pMergeHead;
}

//合并排序数组.....垃圾方法 时间复杂度 就是排序算法的 nlogn   (m+n)log(m+n)
class Solution {
public:
    void merge(vector<int>& nums1, int m, vector<int>& nums2, int n) {
        for (int i = 0; i != n; ++i) {
            nums1[m + i] = nums2[i];//直接合并....
        }
        sort(nums1.begin(), nums1.end());//然后排序...
    }
};

//好一点的方法..... 双指针.... 这个方法也很蠢.......
void merge(vector<int>& nums1, int m, vector<int>& nums2, int n) {
    int p1 = 0, p2 = 0;
    int sorted[m + n];
    int cur;
    while (p1 < m || p2 < n) {//两个指针只要有一个没到边界就进行循环.....

        if (p1 == m) {//如果p1 指针到边了 将v2 的数字取出来..
            cur = nums2[p2++];
        } else if (p2 == n) {//如果 p2 到边了 将来v1 数字取出来....
            cur = nums1[p1++];
        } else if (nums1[p1] < nums2[p2]) {//比较大小....
            cur = nums1[p1++];//保存小值
        } else {
            cur = nums2[p2++];//保存小值
        }
        sorted[p1 + p2 - 1] = cur;//存储..... 数组共有的最小的 一段数... 利用了另一空间......

    }

    for (int i = 0; i != m + n; ++i) {
        nums1[i] = sorted[i];
    }
}

由于本题给出的 nums1 是能够保证其长度能够放得下 m + n 个元素的，所以可以直接把合并的结果放到 nums1 中。

思路一：如果两个数组从开头向结尾（数字从小到大）进行比较，那么每次把比较之后的数字放置到 nums1 中的前面，则需要把 nums1 中第 k 个位置后面的元素向后移动。移动次数比较多。
思路二：如果两个数组从结尾向开头（数字从大到小）进行比较，那么每次把比较之后的数字放置到 nums1 中的后面，由于后面的数字本身就是提供出来的多余的位置，都是 0，因此不需要对 nums1 进行移动。
显然思路二更好。

void merge(vector<int>& nums1, int m, vector<int>& nums2, int n) {
    int p1 = m - 1, p2 = n - 1;
    int tail = m + n - 1;
    int cur;
    while (p1 >= 0 || p2 >= 0) {

        if (p1 == -1) {          //要理解这里的边界条件和思想.........
            cur = nums2[p2--];
        } else if (p2 == -1) {
            cur = nums1[p1--];
        } else if (nums1[p1] > nums2[p2]) {
            cur = nums1[p1--];
        } else {
            cur = nums2[p2--];
        }

        nums1[tail--] = cur;
    }
}


删除倒数第n个节点......

思路一 .........
一种容易想到的方法是，我们首先从头节点开始对链表进行一次遍历，得到链表的长度 L。
随后我们再从头节点开始对链表进行一次遍历，当遍历到第 L−n+1 个节点时，它就是我们需要删除的节点。

为了与题目中的 n 保持一致，节点的编号从 1 开始，头节点为编号 1 的节点。

为了方便删除操作，我们可以从哑节点开始遍历 L−n+1 个节点。当遍历到第 L−n+1 个节点时，它的下一个节点就是我们需要删除的节点，这样我们只需要修改一次指针，就能完成删除操作。


思路二 .........
由于我们需要找到倒数第 n 个节点，因此我们可以使用两个指针 first 和 second 同时对链表进行遍历，并且 first 比 second超前 n个节点。
当 first 遍历到链表的末尾时，second 就恰好处于倒数第 n 个节点。具体地，初始时 first 和 second 均指向头节点。
我们首先使用 first 对链表进行遍历，遍历的次数为 n。
此时，first 和 second 之间间隔了 n−1 个节点，即 first 比 second 超前了 n 个节点。

在这之后，我们同时使用 first 和 second 对链表进行遍历。当 first 遍历到链表的末尾（即 first 为空指针）时，second 恰好指向倒数第 n 个节点。

根据方法一和方法二，如果我们能够得到的是倒数第 n 个节点的前驱节点而不是倒数第 n 个节点的话，删除操作会更加方便。
因此我们可以考虑在初始时将 second 指向哑节点，其余的操作步骤不变。这样一来，当 first 遍历到链表的末尾时，second 的下一个节点就是我们需要删除的节点。


ListNode* removeNthFromEnd(ListNode* head, int n) {
	// 这里为什么 要构造一个节点 你知道为什么吗？ 就是为了解决下面的   {1,2} 2 ==>>{2} 这样的一种特殊情况.........
    ListNode* dummy = new ListNode(0, head);// new ListNode(0); 看平台的构造函数....
    ListNode* first = head;
    ListNode* second = dummy;//这里是为得到 被删除节点的前一个节点......
    for (int i = 0; i < n; ++i) {//这里是先让快指针在前面多跑n次...
        first = first->next;
    }
    while (first) {
        first = first->next;
        second = second->next;
    }
    second->next = second->next->next;
    ListNode* ans = dummy->next;//这里为什么要这样做...这里一定要这样做 有一种存在可能的边界条件.... {1,2} 2 ==>>{2}
    delete dummy;
    return ans;
}



//两个数之和  暴力法则......
vector<int> twoSum(vector<int>& nums, int target) {
        int n = nums.size();
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (nums[i] + nums[j] == target) {
                    return {i, j};
                }
            }
        }
        return {};
}

//两数之和  优化了的时间复杂度.... 空间复杂度为n了
vector<int> twoSum(vector<int>& nums, int target) {
  unordered_map<int, int> hashtable;
  for (int i = 0; i < nums.size(); ++i) {
      auto it = hashtable.find(target - nums[i]);
      if (it != hashtable.end()) {
          return {it->second, i};//...这里就是之前存的数据有结果了 匹配上了所以返回了结果.......
      }
      hashtable[nums[i]] = i;//这里就是存放之前 遍历过的数据 存数据和对应的标记.......
  }
  return {};
}


// 方法总结下 是深度优先还是 广度优先......  入口参数和牛客有区别......
class Solution {
public:
    TreeNode* ans;
    bool dfs(TreeNode* root, TreeNode* p, TreeNode* q) {
        if (root == nullptr) return false;
        bool lson = dfs(root->left, p, q);
        bool rson = dfs(root->right, p, q);
        if ((lson && rson) || ((root->val == p->val || root->val == q->val) && (lson || rson))) {
            ans = root;
        } 
        return lson || rson || (root->val == p->val || root->val == q->val);
    }
    TreeNode* lowestCommonAncestor(TreeNode* root, TreeNode* p, TreeNode* q) {
        dfs(root, p, q);
        return ans;
    }
};



class Solution {
    public TreeNode lowestCommonAncestor(TreeNode root, TreeNode p, TreeNode q) {
        if (root == null || p == root || q == root) {
            return root;
        }

        TreeNode l = lowestCommonAncestor(root.left, p, q);
        TreeNode r = lowestCommonAncestor(root.right, p, q);
    
        return l == null ? r : (r == null ? l : root);//这个 很精彩......  
    }
}

方法二：存储父节点
思路
我们可以用哈希表存储所有节点的父节点，然后我们就可以利用节点的父节点信息从 p 结点开始不断往上跳，
并记录已经访问过的节点，再从 q 节点开始不断往上跳，如果碰到已经访问过的节点，那么这个节点就是我们要找的最近公共祖先。

算法
从根节点开始遍历整棵二叉树，用哈希表记录每个节点的父节点指针。...........
从 p 节点开始不断往它的祖先移动，并用数据结构记录已经访问过的祖先节点。
同样，我们再从 q 节点开始不断往它的祖先移动，如果有祖先已经被访问过，即意味着这是 p 和 q 的深度最深的公共祖先，即 LCA 节点。


class Solution {//...这个方法容易理解.......
public:
    unordered_map<int, TreeNode*> fa;//存放父节点的 map  注意结构定义的时候和牛科上有区别.....
    unordered_map<int, bool> vis;//向上遍历的标记...
    void dfs(TreeNode* root){
        if (root->left != nullptr) {
            fa[root->left->val] = root;
            dfs(root->left);
        }
        if (root->right != nullptr) {
            fa[root->right->val] = root;
            dfs(root->right);
        }
    }
    TreeNode* lowestCommonAncestor(TreeNode* root, TreeNode* p, TreeNode* q) {//这里的入参和返回值和牛课上有区别...
        fa[root->val] = nullptr;//记录所有节点的父节点......根节点的父节点肯定是null啊.....
        dfs(root);//深度遍历记录遍历过程中所有节点的父节点进入 哈希表.....

//两个while 其中一个是记录自己的向上遍历的所有的父节点 另一个就是用来查验的.....
        while (p != nullptr) {
            vis[p->val] = true;//将自己的路径返回的路记录下来...
            p = fa[p->val];//向上遍历......这歌map中记录了自己的父节点.....
        }
        while (q != nullptr) {
            if (vis[q->val]) return q;//这里就是返回公共的祖先节点.......
            q = fa[q->val];//这里也是记录了自己的父节点用来向上寻根的.....
        }
        return nullptr;
    }
};

// 解法一深度遍历 递归解法....
// ........求出根到各个叶的节点路径............
vector<vector<int>> res;
void dfs(TreeNode* root, vector<int> cur, int target){
    if(root == nullptr) return;
    
    cur.push_back(root->val);//递归深度遍历的过程中记录下所有可能的路径节点
    target -= root->val;
    if(target == 0 && root->left == nullptr && root->right == nullptr){//同时满足了这些条件即到了叶子节点了 sum也满足了
        res.push_back(cur);//一维数组存放到二维中去了.....
        return;
    }
    dfs(root->left, cur, target);
    dfs(root->right, cur, target);
}
/**
 * @param root TreeNode类
 * @param sum int整型
 * @return int整型vector<vector<>>
 */
vector<vector<int> > pathSum(TreeNode* root, int sum) {
    dfs(root, {}, sum);//深度遍历......
    return res;
}
//解方二 .... 下次补上.....


------------------------------这样类型的题目 有技巧就用技巧 尽量不要 哈希表去做........
只出现一次的数组 其他都出现两次 所以可以用这个方法........
//异或的原理就是 一 a异或a 等于0  二 a异或0 等于a  三 a⊕b⊕a=b⊕a⊕a=b⊕(a⊕a)=b⊕0=b。
int singleNumber(vector<int>& nums) {
    int ret = 0;
    for (auto e: nums) ret ^= e;
    return ret;
}


只出现一次 其他都是k次 求返回只出现一次的val返回.....
//给你一个整数数组 nums ，除某个元素仅出现 一次 外，其余每个元素都恰出现 k次 。请你找出并返回那个只出现了一次的元素。
// 1哈希表方法 2求和法 3位运算法
哈希表方法就不要说了吧

求和可以考虑

关键是 位运算方法 .......技巧 记住就行不要用错了

int foundOnceNumber(vector<int>& nums, int k) {
    // write code here
    int res = 0;
    for(int i = 0; i < 32; ++i){
        int cnt = 0 ;
        for(auto num : nums){//处理所有数字
            cnt += (num >> i) & 1;//处理每个数 某位进行求和.......
        }
        
        res ^= (cnt%k)<<i;//<<的优先级高于 ^=      某一位数对k 进行求余数  再次返回到固定位置上去......
    }
    return res;
}

//数组中只出现一次的两个数字 其他都是两次
vector<int> singleNumbers(vector<int>& nums) {
    int ret = 0;
    for (int n : nums)
        ret ^= n;

    int div = 1;
    while ((div & ret) == 0) //这里的循环就是找到第一位 不为 0 的位置....   这里很重要 自己理解...
        div <<= 1;

    int a = 0, b = 0;
    for (int n : nums)
        if (div & n)//...........这里知道为什么吗？
          a ^= n;
        else
          b ^= n;

    if(a>b) {
       swap(a,b);
    }

  return vector<int>{a, b};
}



---------------------------------------------------------------------------------------

//有序递增 无重复 数组查找target  返回下标............ 没有则返回 -1 
int search(vector<int>& nums, int target) {
    int left = 0;
    int right = nums.size() - 1; // 定义target在左闭右闭的区间里，[left, right]
    while (left <= right) { // 当left==right，区间[left, right]依然有效，所以用 <=
        int middle = left + ((right - left) / 2);// 防止溢出 等同于(left + right)/2
        if (nums[middle] > target) {
            right = middle - 1; // target 在左区间，所以[left, middle - 1]  //....思考为什么middle可以减去1...... 应为受到while循环条件的影响
        } else if (nums[middle] < target) {
            left = middle + 1; // target 在右区间，所以[middle + 1, right]
        } else { // nums[middle] == target
            return middle; // 数组中找到目标值，直接返回下标
        }
    }
    // 未找到目标值
    return -1;
}

二分法第二种写法
如果说定义 target 是在一个在左闭右开的区间里，也就是[left, right) ，那么二分法的边界处理方式则截然不同。

有如下两点：

while (left < right)，这里使用 < ,因为left == right在区间[left, right)是没有意义的
if (nums[middle] > target) right 更新为 middle，因为当前nums[middle]不等于target，去左区间继续寻找，
而寻找区间是左闭右开区间，所以right更新为middle，即：下一个查询区间不会去比较nums[middle]

int search(vector<int>& nums, int target) {
    int left = 0;
    int right = nums.size(); // 定义target在左闭右开的区间里，即：[left, right)
    while (left < right) { // 因为left == right的时候，在[left, right)是无效的空间，所以使用 <
        int middle = left + ((right - left)/2); //>> 1);
        if (nums[middle] > target) {
            right = middle; // target 在左区间，在[left, middle)中  //...思考为什么 这里 middle不去减1 .....因为为受到while循环条件的影响
        } else if (nums[middle] < target) {
            left = middle + 1; // target 在右区间，在[middle + 1, right)中
        } else { // nums[middle] == target
            return middle; // 数组中找到目标值，直接返回下标
        }
    }
    // 未找到目标值
    return -1;
}





************************************
35. 搜索插入位置 用二分方法 注意技巧 ......         数组最开头的开头  中间 或等于某一个索引  最后的最后
************************************



//java  下次用cpp 改写下.....
 public int[] searchRange(int[] nums, int target) {
        int find = searchRangeHelper(nums, target);
        //如果没找到，返回{-1, -1}
        if (find == -1)
            return new int[]{-1, -1};
        int left = find - 1;
        int right = find + 1;
        //查看前面是否还有target
        while (left >= 0 && nums[left] == target)
            left--;
        //查看后面是否还有target
        while (right < nums.length && nums[right] == target)
            right++;
        return new int[]{left + 1, right - 1};
    }

    //二分法查找
    private int searchRangeHelper(int[] nums, int target) {
        int low = 0;
        int high = nums.length - 1;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            int midVal = nums[mid];
            if (midVal < target) {
                low = mid + 1;
            } else if (midVal > target) {
                high = mid - 1;
            } else {
                return mid;
            }
        }
        return -1;
    }








34 题和牛客上出现的.....     细节较多 还是不容易写对的........
****** 区别于上面的传统的二分查找 就是有 重复有重复元素!!!!!!!!!!!!

--------------------------------------------------------------------
请实现  有重复数字  的升序数组的二分查找
给定一个 元素有序的（升序）整型数组 nums 和一个目标值 target  ，
写一个函数搜索 nums 中的第一个出现的target，如果目标值存在返回下标，否则返回 -1
     ....... 题目明确了用二分法....
---------------------------------------------------------------------

这道题 需要用到上道题的思想.......  注意二分法的利用....... 提升搜索时间复杂度......


int search_rang(vector<int>& nums, int target){//传统二分.....
    int l =0 ,r = nums.size()-1;
    while(l<=r){
        int mid = l+(r-l)/2;//换成移位效率好一点...
        if(nums[mid] < target){
            l = mid + 1;
        }else if(nums[mid] > target){
            r = mid - 1;
        }else{
            return mid;
        }
    }
    return -1;
}

int search(vector<int>& nums, int target) {
    
    int temp = search_rang(nums,target);//利用传统二分法........
    if(temp == -1) return -1;
    
    int left = temp-1,right = temp+1;//知道这里为什么 要减去1 or 加上1吗？ 理解..........
    
    //查看前面是否还有target  .....这里是关键理解了吗？.........
    while(left>=0&& nums[left] == target){//因为是排序的嘛 所以向左边移动 一位看看有没有 target 有就 --
        left--;
    }
    //最后一位同样类似的思想......
    return left+1;//这里为什么  

}





一个数字开平方  要熟练啊闭着眼写出来啊.......
int mySqrt(int x) {

    int l = 0, r = x, ans = -1;

    while (l <= r) {//注意这里的细节...

        int mid = l + (r - l) / 2;//二分逼近.......

        if ((long long)mid * mid <= x) {
            ans = mid;
            l = mid + 1;//右边逼近
        } else {
            r = mid - 1;//左边逼近
        }

    }
    return ans;
}

前面单词 获取 top k 

class Solution {
private:
    struct CmpByValue { //核心，排序条件
        bool operator()(const pair<string,int>& lhs, const pair<string,int>& rhs) {
            if(lhs.second != rhs.second)
                return lhs.second > rhs.second; //次数不同，则按次数大小排序
            else
                return lhs.first < rhs.first; //次数相同，则按单词的字母顺序排序
        }
    };

public:
  vector<string> topKFrequent(vector<string>& words, int k) {
    map<string,int> wordmap;
    vector<string> res;
    for(string word : words)
    {
        wordmap[word]++;
    }
    vector<pair<string,int>> wordvec(wordmap.begin(),wordmap.end());//...看看人家是怎么写的..... map 存放到 pair 容器中.......
    sort(wordvec.begin(), wordvec.end(), CmpByValue());

    for(auto it=wordvec.begin();it!=wordvec.end();it++)
    {
        k--;
        res.push_back(it->first);

        if(k==0) 
        	break;
    }

    return res;
  }

};


//........链表中环的入口点............  
方法一 使用 unordered_map 存储来做处理......
ListNode* EntryNodeOfLoop(ListNode* pHead) {
    unordered_set<ListNode*>nodes;
    //ListNode* pNode = pHead;
    while(pHead){
        if(nodes.find(pHead) == nodes.end()){
            nodes.insert(pHead);
            pHead = pHead->next;
        }else{
            return pHead;
        }
    }
    return nullptr;
}

快指针是慢指针的两倍速 又在c点相遇.. 所以...  2(A+B) = A+B+C+B  其实下面的n计算才是对的.... 这个就是帮助理解的......
等式应更正为 2(A+B)= A+ nB + (n-1)C） 由等式，我们可得，C = A 图解.....https://blog.nowcoder.net/n/deaa284f105e48f49f38b5d7cb809cd7?f=comment
方法二 双指针... 先再快慢指针......再同步伐指针.....

ListNode* EntryNodeOfLoop(ListNode* pHead)
{
    if (pHead==nullptr)//头节点为null情况....
        return nullptr;

    auto slow = pHead;
    auto fast = pHead;
    while (fast && fast->next) {
        fast = fast->next->next;
        slow = slow->next;
        if (fast == slow) { // 有环
            fast = pHead; // 把其中的一个指针重新指向链表头部，另一个不变(还在环内)
            while (slow != fast) { // 这次两个指针一次走一步，相遇的地方就是入口节点
                slow = slow->next;
                fast = fast->next;
            }
            return slow;
        }
    }
    return nullptr;
}

// 两个指针一个fast、一个slow同时从一个链表的头部出发
// fast一次走2步，slow一次走一步，如果该链表有环，两个指针必然在环内相遇
// 此时只需要把其中的一个指针重新指向链表头部，另一个不变(还在环内)，
// 这次两个指针一次走一步，相遇的地方就是入口节点。

// (证明: https://blog.csdn.net/snow_7/article/details/52181049)
// 当相遇的时候，慢指针在环中走了k步，设环之外的部分长为x，环的长度为n，则快指针一共走了x+m*n+k步，(m为快指针在环中走的圈数)，
// 慢指针一共走了x+k步，因为快指针的速度是慢指针的两倍。那么可以得到2(x+k)=x+m*n+k，得到：x = m*n-k，慢指针在圈中还剩下的步数n-k;
// 让快指针从头开始，两个指针每次都走一步，易证:
// 快指针走x+m*n，慢指针走(m-1)*n+n-k=m*n-k=x
// 所以两个指针再次相遇，此刻的节点就是环的入口的节点。

// 这类问题还可以延伸出来求:
// 1. 环的长度: 当快慢指针第一次相遇的时候，把该节点保存下来，让慢指针接着走，当再次到达刚才相遇的节点时所走过的步数就是环的长度。
// 2. 整个链表的长度: 环以外的长度再加上环的长度，就是整个链表的长度
// 3. 两个无环链表第一次相交的公共节点: 先分别求出两个链表的长度，让长的链表先走两个链表长度差的步数，再让两个链表一起走，当走到节点值相同的那个节点时，就是相交的第一个公共节点。


链表反转  每 k 个一组   这一题我觉得有难度  
//https://leetcode-cn.com/problems/reverse-nodes-in-k-group/solution/5fen-dai-ma-ji-jian-cdai-ma-by-fengziluo-ucsi/

class Solution {
public:
    ListNode* reverseKGroup(ListNode* head, int k) {
        auto node = head;
        for (int i=0; i < k; ++i) { // 判断“满载”
            if (node == nullptr) return head;  //这里就是说明了没有满载........
            node = node->next;//
        }
        //满载了 node 就是 第k 节点的下一个节点 作为入参数.........
        auto res = reverse(head, node); // 翻转“满载”车箱
        head->next = reverseKGroup(node, k); // 递归处理下节车箱
        return res; // 返回当前翻转完车箱的车头位置
    }

private:
    // 翻转车箱元素
    ListNode* reverse(ListNode* left, ListNode* right) {
        auto pre = right;//这个节点就是保存...
        while (left != right) {
            auto node = left->next;//保存遍历的下一个节点........
            left->next = pre;//这里反转 连接......
            pre = left;
            left = node;//这两行就是.......  遍历反转............
        }
        return pre;
    }
};


ListNode* reverseKGroup(ListNode* head, int k) {
    ListNode *pre = NULL;
    ListNode *cur = head;
    ListNode *next = NULL;

    for (int i = 0; i < k; i++) {
        if (cur == NULL) {//这样的情况就是 这一组中不满足k个或恰好就是 cur 就是最后一个了....
            return head;
        }
        cur = cur->next;
    }

    cur = head;
    for (int i = 0; i < k; i++) {
        next = cur->next;
        cur->next = pre;
        pre = cur;
        cur = next;
    }

    head->next = reverseKGroup(next, k);

    return pre;
}


//螺旋矩阵 这个方法应该比较牛逼........
class Solution {
public:
    vector<int> spiralOrder(vector<vector<int>>& matrix) {
        vector <int> ans;
        if(matrix.empty()) return ans; //若数组为空，直接返回答案
        int u = 0; //赋值上下左右边界
        int d = matrix.size() - 1;
        int l = 0;
        int r = matrix[0].size() - 1;

        while(true)
        {
            for(int i = l; i <= r; ++i) ans.push_back(matrix[u][i]); //向右移动直到最右
            if(++ u > d) break; //重新设定上边界，若上边界大于下边界，则遍历遍历完成，下同
            for(int i = u; i <= d; ++i) ans.push_back(matrix[i][r]); //向下
            if(-- r < l) break; //重新设定有边界
            for(int i = r; i >= l; --i) ans.push_back(matrix[d][i]); //向左
            if(-- d < u) break; //重新设定下边界
            for(int i = d; i >= u; --i) ans.push_back(matrix[i][l]); //向上
            if(++ l > r) break; //重新设定左边界
        }

        return ans;
    }
};



----------------------------------------------------------------------------
二叉树的三种递归遍历方式..... 要熟练.....
/**
 * struct TreeNode {
 *	int val;
 *	struct TreeNode *left;
 *	struct TreeNode *right;
 * };
 */

class Solution {
public:
    vector<int> preorderTraversal(TreeNode* root) {
        vector<int> result;
        traversal(root, result);//..........在这里调用  递归遍历的方法........
        return result;
    }
};

void traversal(TreeNode* cur, vector<int>& vec) {
    if (cur == NULL) return;
    vec.push_back(cur->val);    // 中
    traversal(cur->left, vec);  // 左
    traversal(cur->right, vec); // 右
}

void traversal(TreeNode* cur, vector<int>& vec) {
    if (cur == NULL) return;
    traversal(cur->left, vec);  // 左
    vec.push_back(cur->val);    // 中
    traversal(cur->right, vec); // 右
}

void traversal(TreeNode* cur, vector<int>& vec) {
    if (cur == NULL) return;
    traversal(cur->left, vec);  // 左
    traversal(cur->right, vec); // 右
    vec.push_back(cur->val);    // 中
}
+++++++++++++++++++++++++++++++++++++++++++++++++++++++

...........非递归遍历 也要会的啊......... 前序 中 后 三种 非递归 借助 栈实现....

    //前.........     根 左右
    vector<int> preorderTraversal(TreeNode* root) { 
        stack<TreeNode*> st;//....区分层序遍历借助的  队列 先进先出思想...............
        vector<int> result;
        if (root == NULL) return result;
        st.push(root);
        while (!st.empty()) {..................注意和层序使用队列遍历的区别!!!!!!!!!!!!!!!!不行就看下层序怎么操作的..........
            TreeNode* node = st.top();                       // 中
            st.pop();
            result.push_back(node->val);
               // 小心哦这里入的是  右......
            if (node->right) st.push(node->right);           // 右（空节点不入栈）
                //..... 这里入的是 左......
            if (node->left) st.push(node->left);             // 左（空节点不入栈）
        }
        return result;
    }

	//中........     .......++++++++  左根右 .............++++++???????? 中序遍历 使用迭代法需要重新理解..........
	vector<int> inorderTraversal(TreeNode* root) {
		vector<int> result;
		stack<TreeNode*> st;
		TreeNode* cur = root;
		while (cur != NULL || !st.empty()) {
		    if (cur != NULL) { // 指针来访问节点，访问到最底层
		        st.push(cur); // 将访问的节点放进栈
		        cur = cur->left;                // 左
		    } else {
		        cur = st.top(); // 从栈里弹出的数据，就是要处理的数据（放进result数组里的数据）
		        st.pop();
		        result.push_back(cur->val);     // 中
		        cur = cur->right;               // 右
		    }
		}
		return result;
	}


    //后序遍历....      ---------  左右根     +++++++ 用这个 根 右 左  反转下就可以了
    vector<int> postorderTraversal(TreeNode* root) {
        stack<TreeNode*> st;
        vector<int> result;
        if (root == NULL) return result;
        st.push(root);
        while (!st.empty()) {
            TreeNode* node = st.top();
            st.pop();
            result.push_back(node->val);
            if (node->left) st.push(node->left); // 相对于前序遍历，这更改一下入栈顺序 （空节点不入栈）
            if (node->right) st.push(node->right); // 空节点不入栈
        }
        reverse(result.begin(), result.end()); // 将结果反转之后就是左右中的顺序了
        return result;
    }


总结: 此时我们用迭代法写出了二叉树的前后中序遍历，大家可以看出前序和中序是完全两种代码风格，并不想递归写法那样代码稍做调整，
就可以实现前后中序。这是因为前序遍历中访问节点（遍历节点）和处理节点（将元素放进result数组中）可以同步处理，但是中序就无法做到同步！



-----------------------------------------------------

// ........一个二叉树求-----(对称)镜像二叉树.......  这里是用了递归写的  还可以用层序和迭代法前序遍历来做这道题.................
    TreeNode* invertTree(TreeNode* root) {

        if (root == NULL) 
        	return root;
        swap(root->left, root->right);  // 中
        invertTree(root->left);         // 左
        invertTree(root->right);        // 右
        return root;
    }
----------------------------------------------------------------------------------

//.......101题  判断一个二叉树是否是 镜像 或者是对称  主要节点上的值是否对称.......  区别上面一题......

//+++++++++++++*********这一题 要多刷 细节有点多****************.............
    bool compare(TreeNode* left, TreeNode* right) {
        // 首先排除空节点的情况
        if (left == NULL && right != NULL) return false;
        else if (left != NULL && right == NULL) return false;
        else if (left == NULL && right == NULL) return true;
        // 排除了空节点，再排除数值不相同的情况
        else if (left->val != right->val) return false;//......将所有可能和不可能的情况都排列出来了........

        // 此时就是：左右节点都不为空，且数值相同的情况  //------------且数值相同
        // 此时才做递归，做下一层的判断
        bool outside = compare(left->left, right->right);   // 左子树：左、 右子树：右   //.......注意这里的参数细节!!!!!!.......
        bool inside = compare(left->right, right->left);    // 左子树：右、 右子树：左   //.......注意这里的细节入口参数!!!!!.....
        bool isSame = outside && inside;                    // 左子树：中、 右子树：中 （逻辑处理）//......同一个层级的判断结果.....
        return isSame;

    }
    bool isSymmetric(TreeNode* root) {
        if (root == NULL) return true;
        return compare(root->left, root->right);//递归..........
    }



// 101 题  使用队列.......... 用堆栈也ok  还是理解上面的那个方法比较容易理解.......
class Solution {//.......怎么感觉用队列解决处理细节更好了呢.................还是用队列这个思想.......会好点........
public:
    bool isSymmetric(TreeNode* root) {
        if (root == NULL) return true;
        queue<TreeNode*> que;
        que.push(root->left);   // 将左子树头结点加入队列
        que.push(root->right);  // 将右子树头结点加入队列
        while (!que.empty()) {  // 接下来就要判断这这两个树是否相互翻转
            TreeNode* leftNode = que.front(); que.pop();
            TreeNode* rightNode = que.front(); que.pop();
            if (!leftNode && !rightNode) {  // 左节点为空、右节点为空，此时说明是对称的
                continue;//.........注意这里的思想.....
            }

            // 左右一个节点不为空，或者都不为空但数值不相同，返回false
            if ((!leftNode || !rightNode || (leftNode->val != rightNode->val))) {
                return false;
            }
            //.........理解下面为什么要那样入队列.....核型.......
            que.push(leftNode->left);   // 加入左节点左孩子
            que.push(rightNode->right); // 加入右节点右孩子

            que.push(leftNode->right);  // 加入左节点右孩子
            que.push(rightNode->left);  // 加入右节点左孩子
        }
        return true;
    }
};

----------------------------------------------------------------
// 100 题目..判断两个 二叉树 是否一样  包括结构和val........
    bool isSameTree(TreeNode* p, TreeNode* q) {
        if(p == NULL && q == NULL)
            return true;
        stack<TreeNode*> s1,s2;
        s1.push(p);
        s2.push(q);
        while(!s1.empty() && !s2.empty()){
            TreeNode* t1 = s1.top();
            TreeNode* t2 = s2.top();
            s1.pop();
            s2.pop();
            if(t1 == NULL && t2 == NULL)
                continue;//.....注意这里的 continue......
            else if(t1 == NULL || t2 == NULL || t1->val != t2->val)//这个地方出过错
                return false;
            s1.push(t1->left);
            s1.push(t1->right);
            s2.push(t2->left);
            s2.push(t2->right); 
        }
        return true;
    }

=============================================================
257. 二叉树的所有路径 
     给定一个二叉树，返回所有从根节点到叶子节点的路径。

这道题目要求从根节点到叶子的路径，/*---所以需要前序遍历---*/，这样才方便让父节点指向孩子节点，找到对应的路径。

在这道题目中将第一次涉及到回溯，因为我们要把路径记录下来，需要回溯来回退一一个路径在进入另一个路径。

前序遍历以及回溯的过程如图：

代码中回溯的思想 看这里的图结合代码理解.........
https://github.com/zhangkele1221/leetcode-master/blob/master/problems/0257.%E4%BA%8C%E5%8F%89%E6%A0%91%E7%9A%84%E6%89%80%E6%9C%89%E8%B7%AF%E5%BE%84.md

我们先使用递归的方式，来做前序遍历。要知道递归和回溯就是一家的，本题也需要回溯。

一 递归函数函数参数以及返回值
  1要传入根节点，
  2记录每一条路径的path，
  3存放结果集的result，这里递归不需要返回值，代码如下：
void traversal(TreeNode* cur, vector<int>& path, vector<string>& result)

二 确定递归终止条件. ---------------这一点很重要!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   再写递归的时候都习惯了这么写：
	if (cur == NULL) {
	    终止处理逻辑
	}
本题的终止条件这样写会很麻烦，因为本题要找到叶子节点，就结束的处理逻辑了 
    是当 cur不为空，其左右孩子都为空的时候，就找到叶子节点

	if (cur->left == NULL && cur->right == NULL) {
	    终止处理逻辑
	}


这里使用vector 结构path来记录路径，所以要把vector 结构的path转为string格式，在把这个string 放进 result里。

那么为什么使用了vector 结构来记录路径呢？ 因为在下面处理单层递归逻辑的时候，要做回溯，使用vector方便来做回溯。

if (cur->left == NULL && cur->right == NULL) { // 遇到叶子节点
    string sPath;
    for (int i = 0; i < path.size() - 1; i++) { // 将path里记录的路径转为string格式
        sPath += to_string(path[i]);
        sPath += "->";
    }
    sPath += to_string(path[path.size() - 1]); // 记录最后一个节点（叶子节点）
    result.push_back(sPath); // 收集一个路径
    return;
}


三 确定单层递归逻辑
因为是前序遍历，需要先处理中间节点，中间节点就是我们要记录路径上的节点，先放进path中。

path.push_back(cur->val);

然后是递归和回溯的过程，上面说过没有判断cur是否为空，那么在这里递归的时候，如果为空就不进行下一层递归了。

所以递归前要加上判断语句，下面要递归的节点是否为空，如下

if (cur->left) {
    traversal(cur->left, path, result);
}
if (cur->right) {
    traversal(cur->right, path, result);
}

此时还没完，递归完，要做回溯啊，因为path 不能一直加入节点，它还要删节点，然后才能加入新的节点。
那么回溯要怎么回溯呢，一些同学会这么写，如下：

if (cur->left) {
    traversal(cur->left, path, result);
}
if (cur->right) {
    traversal(cur->right, path, result);
}
path.pop_back();

这个回溯就要很大的问题，我们知道，回溯和递归是一一对应的，有一个递归，就要有一个回溯，这么写的话相当于把递归和回溯拆开了， 
一个在花括号里，一个在花括号外。

*****************************所以回溯要和递归永远在一起，世界上最遥远的距离是你在花括号里，而我在花括号外！
if (cur->left) {
    traversal(cur->left, path, result);
    path.pop_back(); // 回溯
}
if (cur->right) {
    traversal(cur->right, path, result);
    path.pop_back(); // 回溯
}

class Solution {
private:

    void traversal(TreeNode* cur, vector<int>& path, vector<string>& result) {

        path.push_back(cur->val);//... 将节点路径....入vector....存起来...

        // 这才到了叶子节点
        if (cur->left == NULL && cur->right == NULL) {//....这里是递归结束的 条件注意和传统遍历递归的区别.......
            string sPath;
            for (int i = 0; i < path.size() - 1; i++) {
                sPath += to_string(path[i]);
                sPath += "->";
            }
            sPath += to_string(path[path.size() - 1]);
            result.push_back(sPath);//------- 每一条路径的记录都是一个 string 这里........
            return;
        }

        if (cur->left) {
            traversal(cur->left, path, result);
            path.pop_back(); // 回溯       要理解这里的一行代码...理解...  ............这个就叫回溯............
        }
        if (cur->right) {
            traversal(cur->right, path, result);
            path.pop_back(); // 回溯               ............这个就叫回溯............
        }
    }

public:
    vector<string> binaryTreePaths(TreeNode* root) {
        vector<string> result;
        vector<int> path;
        if (root == NULL) return result;//..............注意这里和传统递归的区别....................

        traversal(root, path, result);
        return result;
    }
};

//.....迭代法.......
class Solution {
public:
    vector<string> binaryTreePaths(TreeNode* root) {
        stack<TreeNode*> treeSt;// 保存树的遍历节点
        stack<string> pathSt;   // 保存遍历路径的节点
        vector<string> result;  // 保存最终路径集合
        if (root == NULL) return result;

        treeSt.push(root);
        pathSt.push(to_string(root->val));

        while (!treeSt.empty()) {
            TreeNode* node = treeSt.top(); treeSt.pop(); // 取出节点 中
            string path = pathSt.top();pathSt.pop();    // 取出该节点对应的路径
            if (node->left == NULL && node->right == NULL) { // 遇到叶子节点
                result.push_back(path);
            }
            if (node->right) { // 右
                treeSt.push(node->right);
                pathSt.push(path + "->" + to_string(node->right->val));
            }
            if (node->left) { // 左
                treeSt.push(node->left);
                pathSt.push(path + "->" + to_string(node->left->val));
            }
        }
        return result;
    }
};

====================================================================================================

617.合并二叉树   ............要理解要真的理解 不是记住了代码........

........前序 中序 后序 都可以实现.....
TreeNode* mergeTrees(TreeNode* t1, TreeNode* t2) {//--------------要理解 不是记忆..............
	//.....这里的递归条件......
        if (t1 == NULL) return t2; // 如果t1为空，合并之后就应该是t2
        if (t2 == NULL) return t1; // 如果t2为空，合并之后就应该是t1

        // 修改了t1的数值和结构
        t1->val += t2->val;                             // 中
        t1->left = mergeTrees(t1->left, t2->left);      // 左    ...........注意这里的入参 和 返回值........
        t1->right = mergeTrees(t1->right, t2->right);   // 右
        
        return t1;//..........这里返回t1 是因为用的就是 t1 ...............
}


{//.....这个递归.....代码比上面的理解起来容易很多............
	    if (t1 == nullptr && t2 == nullptr)
            return nullptr;
        else if (t1 == nullptr)
            return t2;
        else if (t2 == nullptr)
            return t1;
        else {
            TreeNode *t = new TreeNode(t1->val + t2->val);//.........
            t->left = mergeTrees(t1->left, t2->left);//..............递归就是堆栈的压栈的思想...............
            t->right = mergeTrees(t1->right, t2->right);//............
            return t;
        }
}

这里使用队列 来实现的....... 迭代方法研究下........!!!!!!!!!!!!!!!!!!!!!!!!!!???????????????
TreeNode* mergeTrees(TreeNode* t1, TreeNode* t2) {
        if (t1 == NULL) return t2;
        if (t2 == NULL) return t1;
        queue<TreeNode*> que;
        que.push(t1);
        que.push(t2);
        while(!que.empty()) {
            TreeNode* node1 = que.front(); que.pop();
            TreeNode* node2 = que.front(); que.pop();
            // 此时两个节点一定不为空，val相加
            node1->val += node2->val;

            // 如果两棵树左节点都不为空，加入队列
            if (node1->left != NULL && node2->left != NULL) {
                que.push(node1->left);
                que.push(node2->left);
            }
            // 如果两棵树右节点都不为空，加入队列
            if (node1->right != NULL && node2->right != NULL) {
                que.push(node1->right);
                que.push(node2->right);
            }

            // 当t1的左节点 为空 t2左节点不为空，就赋值过去
            if (node1->left == NULL && node2->left != NULL) {
                node1->left = node2->left;
            }
            // 当t1的右节点 为空 t2右节点不为空，就赋值过去
            if (node1->right == NULL && node2->right != NULL) {
                node1->right = node2->right;
            }
        }
        return t1;
}

---- 两个链表生成 相加链表 ---  
假设链表中每一个节点的值都在 0 - 9 之间，那么链表整体就可以代表一个整数。
给定两个这种链表，请生成代表两个整数相加值的结果链表。   要求：空间复杂度 O(n)，时间复杂度 O(n)









要彻底理解动态方程的意义.......
======================================动态规划.... 这样的题目注意写代码的时候写出 1动态规划方程.2初始条件.3遍历和边界.................
//机器人从(0 , 0) 位置出发，到(m - 1, n - 1)终点
//走方格有多少种路径 动态规划.......  思考 不行就去看 gitub笔记讲解............
dp[i][j] = dp[i-1][j]+dp[i][j-1]  方程...

int uniquePaths(int m, int n) {
    vector<vector<int>> dp(m, vector<int>(n, 0));
    for (int i = 0; i < m; i++) dp[i][0] = 1;
    for (int j = 0; j < n; j++) dp[0][j] = 1;
    for (int i = 1; i < m; i++) {
        for (int j = 1; j < n; j++) {
            dp[i][j] = dp[i - 1][j] + dp[i][j - 1];
        }
    }
    return dp[m - 1][n - 1];
}

----------------------------------
矩阵的最小路径和

求最小路径和  dp[i][j]   表示是从i，j 点到 m-1，n-1节点的最小路径和
i,j 只能走到 i+1，j  或者 i，j+1

int minPathSum(vector<vector<int>>& grid) {
    //dp[i][j] = min( dp[i-1][j],dp[i][j])+grid[i][j];
    int cl = grid.size();int row = grid[0].size();
    vector<vector<int>>dp(cl,vector<int>(row));
    dp[0][0] = grid[0][0];
    for(int i=1;i<cl;i++){
        dp[i][0] = dp[i-1][0] + grid[i][0];
    }
    for(int i=1;i<row;i++){
        dp[0][i] = dp[0][i-1] + grid[0][i];
    }

    for(int i=1;i<cl;i++){
        for(int j =1;j<row;j++)
        dp[i][j] = min(dp[i-1][j],dp[i][j-1]) + grid[i][j];
    }
    return dp[cl-1][row-1];
}


//子数组最大累加和
https://github.com/zhangkele1221/leetcode-master/blob/master/problems/0053.%E6%9C%80%E5%A4%A7%E5%AD%90%E5%BA%8F%E5%92%8C%EF%BC%88%E5%8A%A8%E6%80%81%E8%A7%84%E5%88%92%EF%BC%89.md
int maxsumofSubarray(vector<int>& arr) {
    // write code here
    //dp[i] =. max(dp[i-1] + arr[i], arr[i]);  动态方程...
    vector<int> dp(arr.size());   //初始化dp 容器大小
    dp[0] = arr[0];//这里的初始化....
    int result = dp[0];
    for(int i =1;i<arr.size();i++){//..这里遍历的起始位...i = 1;
        dp[i] = max(dp[i-1]+arr[i],arr[i]);
        if(result < dp[i]) result = dp[i];
    }
    return result;
}






//..........   最长重复子数组  子数组连续哦  ------这里是返回的是数字长度......如果要求返回最长的数组怎么处理呢????????
https://mp.weixin.qq.com/s/U5WaWqBwdoxzQDotOdWqZg
dp[i][j] ：以下标i - 1为结尾的A，和以下标j - 1为结尾的B，最长重复子数组长度为dp[i][j]。
此时细心的同学应该发现，那dp[0][0]是什么含义呢？总不能是以下标-1为结尾的A数组吧。

其实dp[i][j]的定义也就决定着，我们在遍历dp[i][j]的时候i 和 j都要从1开始。

--------------根据dp[i][j]的定义，dp[i][j]的状态只能由dp[i - 1][j - 1]推导出来。

--------------即当A[i - 1] == B[j - 1] 相等的时候，dp[i][j] = dp[i - 1][j - 1] + 1;

根据递推公式可以看出，遍历i 和 j 要从1开始！结尾也要注意 

int findLength(vector<int>& A, vector<int>& B) {
    vector<vector<int>> dp (A.size() + 1, vector<int>(B.size() + 1, 0));//........注意这里的初始化细节......
        int result = 0;
        for (int i = 1; i <= A.size(); i++) {
            for (int j = 1; j <= B.size(); j++) {
                if (A[i - 1] == B[j - 1]) {//.......这里是核心啊......
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                }
                if (dp[i][j] > result) result = dp[i][j];
            }
        }
    return result;
}

string  findLength(vector<int>& A, vector<int>& B) {
    vector<vector<int>> dp (A.size() + 1, vector<int>(B.size() + 1, 0));//........注意这里的初始化细节......
        int result = 0;//记录的是最大长度
        int indx = 0; //最长子串的最后一个idx 
        for (int i = 1; i <= A.size(); i++) {
            for (int j = 1; j <= B.size(); j++) {
                if (A[i - 1] == B[j - 1]) {//.......这里是核心啊......
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                }
                if (dp[i][j] > result) {
                	indx = i-1;//记录最长子串最后一个字符的数组下标
                	result = dp[i][j];
                }
            }
        }
    return string sbu(A.begin()+indx-result+1,A.begin()+indx+1);
}

//.......... 最长公共子序列-II   ....子序列...//...区分...
// 两种考法。一种是返回最长长度。一种是返回找出的序列
//长度为[0, i - 1]的字符串text1与长度为[0, j - 1]的字符串text2的最长公共子序列长度为dp[i][j]
//所以接下来处理要处理。s1[i - 1] == s2[j - 1]相等和不等情况.... 来得出动态方程.....
    string LCS(string s1, string s2) {
        // write code here
        int n = s1.size();
        int m = s2.size();
        int result = 0;
        vector<vector<int>> dp(n+1, vector<int>(m+1, 0));//n+1 m+1理解
        for (int i = 1; i <= s1.size(); i++) {//i=1理解的吧...
            for (int j = 1; j <= s2.size(); j++) {
                if (s1[i - 1] == s2[j - 1]) {//相等
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                } else {//不等
                    dp[i][j] = max(dp[i - 1][j], dp[i][j - 1]);
                }
                if(result < dp[i][j])
                    result = dp[i][j];//这里就是记录了最长的长度
            }
        }
        if(!result) return "-1";
        //返推结果.....
        string s;
        while(n>0&&m>0){//这里其实处理了i=0,j=0的，对应公式0的反推场景
            if(s1[n-1] == s2[m-1]){//反推公式中相等的场景
                s+=s1[n-1]; //该值一定是被选取到的，根据之前的公式，知道两条字符串的下标都前进一位
                n--;
                m--;
            }else{//对应公式中不相等的反推场景
                if(dp[n][m-1] > dp[n-1][m]){//找大的那个方向，此处是左边大于上面，则该处的结果是来自左边
                    m--;
                }else if(dp[n][m-1] < dp[n-1][m]){
                    n--;
                }else if(dp[n][m-1] == dp[n-1][m]){
                    //对于有分支的可能时，我们选取单方向
                    n--;//这里n-- m--都OK//此结果对于结果1所选取方向，str1的下标左移一位.替换为j--，则结果对应与结果2选取的方向
                }
            }
        }
        reverse(s.begin(), s.end());
        return    s;
    }



//回文字符串....
回文子串个数
最大回文子串
//dp[i][j]：表示区间范围[i,j] （注意是左闭右闭）的子串是否是回文子串，如果是dp[i][j]为true，否则为false。
//. s[i] == s[j]  就是判断 相等和不等 两种情况. 不等肯定不是 dp[i][j]=false   等了还要分情况讨论  是不是 true  j-i <=1  
    int result =0;
    if(s[i] == s[j]){
        if(j-i<=1)
            dp[i][j] = true;
            result++;
        else if(dp[i+1][j-1]){//这里为什么要 i+1 和 j-1 进行判断 自己理解
            dp[i][j] = true;
            result++;
        }
    }

int countSubstrings(string s) {
	vector<vector<bool>> dp(s.size(), vector<bool>(s.size(), false));//dp[i][j]：表示区间范围[i,j]（注意是左闭右闭)的子串是否是回文子串
	int result =0;
	for(int i = s.size()-1;i>=0;i--){//...i 是从大到小开始的哈.....为什么从大到小呢自己想... dp[i+1][j-1]-->>dp[i][j] 
		for(int j = i;j<s.size();j++){//... j 为什么等于i 就不要多说了吧.....
	        if(s[i] == s[j]){
	            if(j-i<=1){
	                dp[i][j] = true;
	                result++;
	            }
	            else if(dp[i+1][j-1]){// 这个i 和 j 的边界推断关系是靠  i 向前移动  j是向后移动   
	                dp[i][j] = true;
	                result++;
	            }
	        }
		}
	}
	return result;
}

//................................................//
int getLongestPalindrome(string A, int n) {
    //dp[i][j] ==true 是回文....
    vector<vector<bool>> dp(n,vector<bool>(n,false));
    string sub1;
    for(int i=n-1;i>=0;i--){
        for(int j=i;j<n;j++){
             if(A[i] == A[j]){
                 if((j-i)<=1){
                    dp[i][j] = true;
		                    string sub(A.begin()+i,A.begin()+j+1);
		                    if(sub.size()>sub1.size())
		                        sub1 = sub;
                 }
                 else if(dp[i+1][j-1]){
                    dp[i][j] = true;
		                    string sub(A.begin()+i,A.begin()+j+1);
		                        if(sub.size()>sub1.size())
		                            sub1 = sub;
                 }
                 
             }
        }
    }
  return sub1.size();
}
//...............................................//

//............最长递增子序列  最长递增子序列  较难..26%通过. ..真的较难   返回长度肯定OK啊.........!!!!!
// 两种出题形式: 
//一种是返回序列 
//一种是返回长度  这个会 返回序列也很简单了 自己想啊！ 记录长的长度的最后一个序列indx 然后在往回遍历将按照大小和数量其取出来从数组中

dp[i]表示i之前包括i的最长上升子序列。

位置i的最长升序子序列等于j从0到i-1各个位置的最长升序子序列 + 1 的最大值。

if (nums[i] > nums[j]) dp[i] = max(dp[i], dp[j] + 1);//...这个动态方程是有前提条件的哈......



确定遍历顺序
dp[i] 是有0到i-1各个位置的最长升序子序列 推导而来，那么遍历i一定是从前向后遍历。

j其实就是0到i-1，遍历i的循环里外层，遍历j则在内层，代码如下：

for (int i = 1; i < nums.size(); i++) {
    for (int j = 0; j < i; j++) {//.........注意这里的 j < i 细节.......
        if (nums[i] > nums[j]) dp[i] = max(dp[i], dp[j] + 1);
    }
    if (dp[i] > result) result = dp[i]; // 取长的子序列
}
//最好是理解透了这个写法.......
int lengthOfLIS(vector<int>& nums) {
    int n = nums.size();
    if(n<=1) return n;
    vector<int> dp(n,1);
    int result = 1;
    for(int i = 1;i<n;i++){
        for(int j=0;j<i;j++){
            if(nums[i]>nums[j]) dp[i] = max(dp[i],dp[j]+1);//这里为什么这样写一定一定要理解......
        }
        if(result<dp[i])
            result = dp[i];
    }
    return result;
}

//这个比较容易理解...就是分了两步骤..........
int lengthOfLIS(vector<int>& nums) {
    int n = nums.size();
    if(n<=1) return n;
    vector<int> dp(n,1);
    int result = 1;
    for(int i = 1;i<n;i++){
        int cnt = 1;//...这里表示 以i结尾的nums[i]的 最长子序列 dp[i]  最长子序列最小也是 1啊！ 就是nums[i]自己.......
        for(int j=i-1;j>=0;j--){
            if(nums[j]<nums[i]){//...这样都是为了便于理解.....
                cnt = max(cnt,dp[j]+1);//你看这个就比上面的那个容易理解了............
            }
        }
        dp[i] = cnt;//.....这里就是分为两步来干了....

        if(result<dp[i]){
        	result = dp[i];
        }

    }
    return result;
}


vector<int> LIS(vector<int>& arr) {
    // write code here
    //dp[i]表示i之前包括i的最长上升子序列。
    //位置i的最长升序子序列等于j从0到i-1各个位置的最长升序子序列 + 1 的最大值。
    //所以：if (nums[i] > nums[j]) dp[i] = max(dp[i], dp[j]+1);//这个特别重要需要理解.......
    int n = arr.size();
    if(n<=1)return {arr[0]};
    vector<int> dp(n);
    dp[0] = 1;
    int indx = 0;
    int result = 1;
    for(int i =1;i<n;i++){
        for(int j=0;j<i;j++){
            if(arr[i]>arr[j]) dp[i] = max(dp[i], dp[j]+1);
            if(result<dp[i]) {
                indx = i;
                result = dp[i];
            }
        }
    }
    vector<int> v(result);
    int temp = arr[indx];
    v[--result]=temp;
    
    for(int j=indx-1;j>=0;j--)
    {
        if((dp[indx]==dp[j]+1)&&(arr[indx]>arr[j]))
        {
            //从后向前,将属于递增子序列的元素加入到subArr中。
            v[--result]=arr[j];
            indx=j;
        }
    }
    /*
    for(int i = indx-1;i>=0;i--){
        if(  &&(arr[i] < temp )&& (--result >=0)){
            temp = arr[i];
            v[result] = temp;
        }
    }*/
    return v;
}



//最长连续递增序列    ----数组是未排序的.....
int findLengthOfLCIS(vector<int>& nums) {
    if (nums.size() == 0) return 0;
    int result = 1;
    vector<int> dp(nums.size() ,1);
    for (int i = 0; i < nums.size() - 1; i++) {//....注意边界...
        if (nums[i + 1] > nums[i]) { // 连续记录
            dp[i + 1] = dp[i] + 1;
        }
        if (dp[i + 1] > result) result = dp[i + 1];
    }
    return result;
}

//子数组的最大累加和问题
int maxsumofSubarray(vector<int>& arr) {
    // write code here
    //dp[i] = max(dp[i-1]+arr[i],arr[i])
    vector<int> dp(arr.size());
    dp[0] = arr[0];
    int result = dp[0];
    for(int i = 1;i<arr.size();i++){
        dp[i] = max(dp[i-1]+arr[i],arr[i]);
        if(dp[i]>result)
            result = dp[i];
    }
    return result;
}

//力扣.....
动态规划：斐波那契数
动态规划：爬楼梯
动态规划：使用最小花费爬楼梯
动态规划：不同路径

动态规划：买卖股票的最佳时机

动态规划：最长递增子序列    if(num[i]>nums[j]) dp[i] = max(dp[i],dp[j]+1) 
动态规划：最长连续递增序列   if(num[i+1]>nums[i]) dp[i+1] = dp[i] + 1;
动态规划：最长重复子数组==最长公共子串==包括求出公共的串     if (A[i - 1] == B[j - 1])  dp[i][j] = dp[i - 1][j - 1] + 1;
动态规划：最长公共子序列 (注意和上一题区别)  if(A[i-1]==B[j-1]) dp[i][j]=dp[i-1][j-1]+1; else dp[i][j]=max(dp[i-1][j],dp[i][j-1]) 
动态规划：不相交的线
动态规划：最大子序和         dp[i] = max(dp[i-1]+arr[i],arr[i])
动态规划：判断子序列
动态规划：不同的子序列
动态规划：两个字符串的删除操作
动态规划：编辑距离  最小编辑代价    
动态规划：回文子串
动态规划：最长回文子序列

//牛扣....


//接水问题   两种做法.... 双指针 动态规划

//.....动态规划.....
long long maxWater(vector<int>& height) {
    if (height.size() <= 2) return 0;
    vector<int> maxLeft(height.size(), 0);
    vector<int> maxRight(height.size(), 0);
    int size = maxRight.size();

    // 记录每个柱子左边柱子最大高度
    maxLeft[0] = height[0];
    for (int i = 1; i < size; i++) {
        maxLeft[i] = max(height[i], maxLeft[i - 1]);
    }
    // 记录每个柱子右边柱子最大高度
    maxRight[size - 1] = height[size - 1];
    for (int i = size - 2; i >= 0; i--) {
        maxRight[i] = max(height[i], maxRight[i + 1]);
    }
    // 求和
    long long sum = 0;
    for (int i = 0; i < size; i++) {
        long long count = min(maxLeft[i], maxRight[i]) - height[i];
        if (count > 0) sum += count;
    }
    return sum;
}

//......双指针.....
long long maxWater(vector<int>& arr) {
    long long sum = 0;
    for (int i = 1; i < arr.size()-1; i++) {
        // 第一个柱子和最后一个柱子不接雨水

        int rHeight = arr[i]; // 记录右边柱子的最高高度
        int lHeight = arr[i]; // 记录左边柱子的最高高度
        for (int r = i + 1; r < arr.size(); r++) {
            if (arr[r] > rHeight) rHeight = arr[r];
        }
        for (int l = i - 1; l >= 0; l--) {
            if (arr[l] > lHeight) lHeight = arr[l];
        }
        long long h = min(lHeight, rHeight) - arr[i];
        if (h > 0) sum += h;
    }
    return sum;
}





========================================================================
........................链表.................
移除头结点和移除其他节点的操作是不一样的，因为链表的其他节点都是通过前一个节点来移除当前节点，而头结点没有前一个节点。

所以头结点如何移除呢，其实只要将头结点向后移动一位就可以，这样就从链表中移除了一个头结点。


https://camo.githubusercontent.com/0fcf4df78daa19beaef5d769abb81f303ef7f6280f78aa4f01e97caeb3ee53ec/68747470733a2f2f696d672d626c6f672e6373646e696d672e636e2f32303231303331363039353631393232312e706e67
这里来给链表添加一个虚拟头结点为新的头结点，此时要移除这个旧头结点元素1。

这样是不是就可以使用和移除链表其他节点的方式统一了呢？

来看一下，如何移除元素1 呢，还是熟悉的方式，然后从内存中删除元素1。

最后呢在题目中，return 头结点的时候，别忘了 return dummyNode->next;， 这才是新的头结点!!!!!!!!!!这句话是重点 记住了！！！！！！


//搜索 回溯  ............
给你一个由 '1'（陆地）和 '0'（水）组成的的二维网格，请你计算网格中岛屿的数量。
岛屿总是被水包围，并且每座岛屿只能由水平方向和/或竖直方向上相邻的陆地连接形成。
此外，你可以假设该网格的四条边均被水包围。

看看这个视频动态图就应该可以理解了为什么递归....了
https://www.bilibili.com/video/BV1Mt4y127V4?from=search&seid=13906954210818295867&spm_id_from=333.337.0.0

void dfs(vector<vector<char>>& grid, int r, int c) {
    int nr = grid.size();//获取数组的行数 列数
    int nc = grid[0].size();

    grid[r][c] = '0';// 将当前空格数设置为 ‘0’ 表示已经遍历过了  司考这里为什么要这么做呢？走过的为 ‘0’ 一定要重置0  优化加速这里....
    //遍历上下左右四个 并且要递归......
    if (r - 1 >= 0 && grid[r-1][c] == '1') dfs(grid, r - 1, c);//....这里为什么要递归呢？
    if (r + 1 < nr && grid[r+1][c] == '1') dfs(grid, r + 1, c);//为什么要递归自己想就懂了.....
    if (c - 1 >= 0 && grid[r][c-1] == '1') dfs(grid, r, c - 1);
    if (c + 1 < nc && grid[r][c+1] == '1') dfs(grid, r, c + 1);
}

int numIslands(vector<vector<char>>& grid) {
    int nr = grid.size();
    if (!nr) return 0;
    int nc = grid[0].size();//获得数组的行列数.............

    int num_islands = 0;
    for (int r = 0; r < nr; ++r) {
        for (int c = 0; c < nc; ++c) {
            if (grid[r][c] == '1') {//这里是 1 岛屿数量就要加一哦........
                ++num_islands;
                dfs(grid, r, c);//深度遍历.....
            }
        }
    }

    return num_islands;
}


//螺旋打印一个二维数组 
vector<int> spiralOrder(vector<vector<int>>& matrix) {
    vector<int> v;
    int num = matrix.size();
    if(num == 0) return v;
    int d = num-1;
    int u =0;
    int l =0;
    int r = matrix[0].size()-1;
    
    while(1){
        for(int i =l;i<=r;i++)v.push_back(matrix[u][i]);
        if(++u>d)break;//左到右 上边界
        for(int i =u;i<=d;i++)v.push_back(matrix[i][r]);
        if(--r<l)break;//上到下 右边界
        for(int i =r;i>=l;i--)v.push_back(matrix[d][i]);
        if(--d<u)break;//右到左 下边界
        for(int i =d;i>=u;i--)v.push_back(matrix[i][l]);
        if(++l>r)break;//下到上 左边界
    }
    return v;
}

// 顺时针旋转矩阵
vector<vector<int> > rotateMatrix(vector<vector<int> > mat, int n) {
        // write code here
    vector<vector<int>> A(n,vector<int>(n,0));
    for(int i =0;i<n;i++){// 逐行翻转
        for(int j =0;j<n;j++){// 逐列处理元素
            A[j][n-1-i] = mat[i][j];//对于矩阵中第 ii 行的第 jj 个元素，在旋转后，它出现在倒数第 ii 列的第 jj 个位置。
        }
    }
    return A;
}



//三数之和为0 将他们输出 且不要重复...........
vector<vector<int>> threeSum(vector<int>& nums) {
           vector<vector<int>> res;
    auto len = nums.size();
    if(len==0) return res;
    sort(nums.begin(),nums.end());
    for (int i = 0; i < len; i++) {
        if (nums[i] > 0) break;//这里是优化
        int target = -nums[i];
        int l = i + 1;
        int r = len - 1;
        if (i == 0 || nums[i] != nums[i - 1]) {//...这里是关键....... i =0 直接进去处理 如果i不是0 那就要 判断下前后的大小 排除大小重复的出现....
            while (l < r) {
                if (nums[l] + nums[r] == target) {
                    res.push_back({nums[i], nums[l], nums[r]});
                    while (l < r && nums[l] == nums[l + 1]) l++;
                    while (l < r && nums[r] == nums[r - 1]) r--;
                    l++;
                    r--;
                } else if (nums[l] + nums[r] < target)
                    l++;
                else
                    r--;
            }
        }
    }
    return res;
}

// 还有两个细节没有考虑到 就是 k =0 或者 num.size()
//滑动窗口最大值 窗口的大小为 k    -----难题...  多做几遍吧理解 细节....
// 视频讲解 https://www.bilibili.com/video/BV1H5411j7o6?from=search&seid=16169035466697175430&spm_id_from=333.337.0.0
class Solution {
private:
    class MyQueue { //单调队列（从大到小）
    public:
        deque<int> que; // 使用deque来实现单调队列
        // 每次弹出的时候，比较当前要弹出的数值是否等于队列出口元素的数值，如果相等则弹出。
        // 同时pop之前判断队列当前是否为空。
        void pop(int value) {
            if (!que.empty() && value == que.front()) {
                que.pop_front();
            }
        }
        // 如果push的数值大于入口元素的数值，那么就将队列后端的数值弹出，直到push的数值小于等于队列入口元素的数值为止。
        // 这样就保持了队列里的数值是单调从大到小的了。
        void push(int value) {
            while (!que.empty() && value > que.back()) {//..............这里只要后面插入一个大的前面的全部干掉....
                que.pop_back();
            }
            que.push_back(value);

        }
        // 查询当前队列里的最大值 直接返回队列前端也就是front就可以了。
        int front() {
            return que.front();//...这里只是返回第一个元素 并不是弹出........
        }
    };
public:
    vector<int> maxSlidingWindow(vector<int>& nums, int k) {
        MyQueue que;
        vector<int> result;
        for (int i = 0; i < k; i++) { // 先将前k的元素放进队列
            que.push(nums[i]);
        }
        result.push_back(que.front()); // result 记录前k的元素的最大值 ----这一步很重要啊 不要忘记了....
        for (int i = k; i < nums.size(); i++) {
            que.pop(nums[i - k]); //滑动窗口移除最前面元素---这里很关键就是窗口向右移动了如果是窗口前面的值那就需要出队列 ..nums[i - k]  这里的一步需要理解了.......
            que.push(nums[i]); // 滑动窗口前加入最后面的元素
            result.push_back(que.front()); // 记录对应的最大值
        }
        return result;
    }
};

//......接雨水问题.......  还是单调栈 上面是单调队列解决的......


//表达式求值 

请写一个整数计算器，支持加减乘三种运算和括号。

    /** 
     string str="We think in generalities, but we live in details.";
     string str2 = str.substr (3,5); // "think"
     * 代码中的类名、方法名、参数名已经指定，请勿修改，直接返回方法规定的值即可
     * 返回表达式的值
     * @param s string字符串 待计算的表达式
     * @return int整型
     */
int solve(string s) {
    // write code here
    int res = 0; //用于返回当前字符串的计算结果
    stack<int> sum; //用于求和
    char sign = '+'; //记录前一个符号，初始化为+，因为可以看成当前字符串前先加0
    int num = 0; //用于将连续数字字符串转化成数字或者记录递归结果
    for(int i = 0; i < s.size(); i++) { //遍历每一个字符
        if(s[i] >= '0' && s[i] <= '9') //先处理数字字符
            num = 10 * num + s[i] - '0'; //进位后加个位数

            if(s[i] == '(') { //对于括号
                int left = i++, count = 1; //用left记录最左括号位置，count记录左括号数，i当成右指针右移一格
                while(count > 0) { //最终目的是找到与最左括号匹配的右括号，类似于栈操作
                    if(s[i] == '(') count++;
                    else if(s[i] == ')') count--;
                    i++;
                }
                num = solve(s.substr(left + 1, i - left - 2)); //迭代计算括号内数值，注意不要包含最左最右括号，不然会死循环
                i--; //此时i是最右括号下一位，需要左移一位防止最右括号在字符串尾时发生越界从而使下面的判定失效
            }

        if(i == s.size() - 1 || s[i] == '+' || s[i] == '-' || s[i] == '*') { //对于字符串尾，或者加减乘，此时我们用的符号是上一次的，结算当前数字
            if(sign == '+') sum.push(num); //加法入栈
            else if(sign == '-') sum.push(-num); //减法相当于加负数
            else if(sign == '*') sum.top() *= num; //乘法与栈顶相乘
            sign = s[i]; //更新符号，若为末尾的右括号也无妨，因为马上就退出循环了
            num = 0; //重置当前数
        }
    }

    while(!sum.empty()) { //将栈内所有数字相加
        res += sum.top();
        sum.pop();
    }
    return res; //返回当前字符串计算结果
}

NC21 链表内指定区间反转  不简单哦 通过率不高........



//---------要吧最近刷的题目都整理一下啦  看下 刷题记录....牛客...

合并区间
/**
 * Definition for an interval.
 * struct Interval {
 *     int start;
 *     int end;
 *     Interval() : start(0), end(0) {}
 *     Interval(int s, int e) : start(s), end(e) {}
 * };
 */
/*
对左边界排序，如果下一个区间的左边界在前一个的有边界内，考虑是否要更新边界，
如果如果下一个区间的左边界在前一个的有边界外，说明区间无法合并，开始计算下一个区间
*/
class {
vector<Interval> merge(vector<Interval> &intervals) {
    vector<Interval> result;
    int l = intervals.size();
    if(l==0) return result;
    
    sort(intervals.begin(), intervals.end(), cmp);//区间和区间排序  区间中自己的元素是排好序的
    result.push_back(intervals[0]);//放入 第一个区间
    
    for(int i=1;i<l;i++)
    {
        if(result.back().end >= intervals[i].start)//如果区间的最后一个元素  大于等于 下一个区间的第一个元素 那就需要合并了...
            result.back().end = max(result.back().end, intervals[i].end);//合并的时候 注意都是拿出前后区间  中区间最后一个元素的最大值存入
        else
            result.push_back(intervals[i]);//如果 区间的最后一个小于下一个区间的首位 那就符合条件直接追加.....
    }         
    return result;
}

static bool cmp(const Interval &a, const Interval &b)
{
    return a.start < b.start;     
}

};       

-------这几题都是树的要和树放一起去......方便归纳........
........这一题通过率不高哦.....
请判断该二叉树是否为搜索二叉树和完全二叉树   -------用了什么思想很关键.....
class Solution {
public
    bool check_perfect(TreeNode*root){//完全二叉树的特点........
        if(root==nullptr) return true;
        queue<TreeNode*> q;
        bool reachend = false;
        q.push(root);
        while(!q.empty()){
            TreeNode* cur = q.front();
            q.pop();
            if( reachend && cur !=nullptr){//tag 是最后了 但是cur 不是空那就是非完全二叉树了
                return false;
            }
            if(cur == nullptr){//当前节点是空节点了  那就是最后一个了打上tag 
                reachend = true;
                continue;
            }
            q.push(cur->left);//....这里为什么不做判断就push 了要知道为什么......
            q.push(cur->right);//注意这里与层序遍历的判断条件不同的是这里无需判断 为什么呢？
        }
        return true;
    }
   vector<int> v;
    void travel(TreeNode*root){//知道用什么思想吗？知道 二叉搜索树的特点.....
        if(root==nullptr) return ;
        travel(root->left);
        v.push_back(root->val);
        travel(root->right);
    }
    bool check(vector<int>&v){
        if(v.empty()) return true; 
        for(int i =1;i<v.size();i++){
            if(v[i]<=v[i-1])return false;
        }
        return true;
    }
    vector<bool> judgeIt(TreeNode* root) {//这道题其实就是两道题来的......
        // write code here
        bool flag1 = check_perfect(root);
        travel(root);
        bool flag2 = check(v);
        return {flag2,flag1};
    }
};

NC81 二叉搜索树的第k个结点 

vector<TreeNode*> v;
void travel(TreeNode*root){
    if(root==nullptr) return ;
    travel(root->left);
    v.push_back(root);
    travel(root->right);
}
TreeNode* KthNode(TreeNode* pRoot, int k) {
    if(pRoot==nullptr) return nullptr;
    travel(pRoot);
    if(k==0) return nullptr;
    return v[k-1];
}



删除重复连续的重复字符   ........感觉题意有问题.....
string removeDuplicates(string S) {
    string result;
    for(char s : S) {
        if(result.empty() || result.back() != s) {
            result.push_back(s);
        }
        else {
            result.pop_back();
        }
    }
    return result;
}




KMP算法
什么叫前缀 什么叫后缀？理解的吧！

前缀是指不包含最后一个字符的所有以第一个字符开头的连续子串。

后缀是指不包含第一个字符的所有以最后一个字符结尾的连续子串。

那么什么是前缀表：(记录下标i之前（包括i）的字符串中)，有多大长度的相同前缀后缀。

因为前缀表要求的就是相同前后缀的长度。



c++八股 未看......
https://www.nowcoder.com/discuss/630127?type=all&order=time&pos=&page=1&ncTraceId=&channel=-1&source_id=search_all_nctrack
https://www.nowcoder.com/discuss/678033?type=all&order=time&pos=&page=1&ncTraceId=&channel=-1&source_id=search_all_nctrack
https://www.nowcoder.com/discuss/624096?type=post&order=time&pos=&page=1&ncTraceId=&channel=-1&source_id=search_post_nctrack
https://www.nowcoder.com/discuss/593655?type=post&order=time&pos=&page=2&ncTraceId=&channel=-1&source_id=search_post_nctrack
https://www.nowcoder.com/discuss/510707?type=post&order=time&pos=&page=1&ncTraceId=&channel=-1&source_id=search_post_nctrack
https://www.nowcoder.com/discuss/490304?type=post&order=time&pos=&page=1&ncTraceId=&channel=-1&source_id=search_post_nctrack
https://www.nowcoder.com/discuss/494239?type=post&order=time&pos=&page=1&ncTraceId=&channel=-1&source_id=search_post_nctrack
https://www.nowcoder.com/discuss/480356?type=post&order=time&pos=&page=1&ncTraceId=&channel=-1&source_id=search_post_nctrack



什么是内存泄漏 什么是内存溢出？

泄漏就是申请的内存没有被释放

溢出就是 内存不够用了 程序要求的内存超过了系统分配的范围

从上面可以看出内存泄漏堆积最终可能会导致内存溢出.....


c++ 八股文代码补充
智能指针实现 string实现 vector 实现 都是简单实现 

shared_ptr 线程安全问题  说白了就是 智能指针引用计数是原子操作是线程安全的 
但是读写指针指向的内容 进行多线程操作就不是安全的了 需要锁..... 也就是对shred_ptr 本身进行读写操作是不安全的
网站分析  https://cloud.tencent.com/developer/article/1654442
#include<iostream>
#include<mutex>
#include<thread>
using namespace std;
// 因为我们这里的引用计数不是原子的所以用了粗暴的方式 加锁操作了 
template<class T>
class Shared_Ptr{
private:
    int *_pRefCount;
    T* _pPtr;
    mutex* _pMutex;
    void Release()//...细节都在这里.......
    {
        bool deleteflag = false;
        _pMutex->lock();
        if (--(*_pRefCount) == 0)
        {
            delete _pRefCount;
            delete _pPtr;
            deleteflag = true;
        }
        _pMutex->unlock();
        if (deleteflag == true)//这里用来释放锁的....
            delete _pMutex;
    }
    void AddRefCount()
    {
        _pMutex->lock();
        ++(*_pRefCount);
        _pMutex->unlock();
    }
public:
    //构造
    Shared_Ptr(T* ptr = nullptr):_pPtr(ptr), _pRefCount(new int(1)), _pMutex(new mutex){}
    //析构
    ~Shared_Ptr(){
        Release();
    }
    //copy构造
    Shared_Ptr(const Shared_Ptr<T>& sp):_pPtr(sp._pPtr), _pRefCount(sp._pRefCount), _pMutex(sp._pMutex){
        AddRefCount();//加计数......
    }
    //赋值
    Shared_Ptr<T>& operator=(const Shared_Ptr<T>& sp)
    {
        //if (this != &sp)
        if (_pPtr != sp._pPtr)
        {
            // 释放管理的旧资源
            Release();
            // 共享管理新对象的资源，并增加引用计数
            _pPtr = sp._pPtr;
            _pRefCount = sp._pRefCount;
            _pMutex = sp._pMutex;
            AddRefCount();
        }
        return *this;
    }
    T& operator*(){
        return *_pPtr;
    }
    T* operator->(){
        return _pPtr;
    }
    int UseCount() { return *_pRefCount; }
    T* Get() { return _pPtr; }
};




设计LRU缓存结构，该结构在构造时确定大小，假设大小为K，并有如下两个功能
set(key, value)：将记录(key, value)插入该结构
get(key)：返回key对应的value值
[要求]
set和get方法的时间复杂度为O(1)
某个key的set或get操作一旦发生，认为这个key的记录成了最常使用的。
当缓存的大小超过K时，移除最不经常使用的记录，即set或get最久远的。
若opt=1，接下来两个整数x, y，表示set(x, y)
若opt=2，接下来一个整数x，表示get(x)，若x未出现过或已被移除，则返回-1
对于每个操作2，输出一个答案



c++八股文 

内存泄漏分类：

堆内存泄漏（heap leak）。堆内存值得是程序运行过程中根据需要分配通过malloc\realloc\new等从堆中分配的一块内存，再完成之后必须要通过调用对应的free或者delete删除。
如果程序的设计的错误导致这部分内存没有被释放，那么此后这块内存将不会被使用，就会产生Heap Leak。
系统资源泄露（Resource Leak）。
主要指程序使用系统分配的资源比如 Bitmap，handle，SOCKET等没有使用相应的函数释放掉，导致系统资源的浪费，严重可导致系统效能降低，系统运行不稳定。
没有将基类的析构函数定义为虚函数。
当基类指针指向子类对象时，如果基类的析构函数不是virtual，那么子类的析构函数将不会被调用，子类的资源没有正确的释放，从而造成内存泄漏。

1. 段错误是什么
一句话来说，段错误是指访问的内存超出了系统给这个程序所设定的内存空间，例如访问了不存在的内存地址、
访问了系统保护的内存地址、访问了只读的内存地址等等情况。这里贴一个对于“段错误”的准确定义。

2.段错误产生的原因
访问不存在的内存地址
访问系统保护的内存地址
访问只读的内存地址
栈溢出 递归调用



https://blog.csdn.net/qq_31349683/article/details/112381183

右值引用 就是减少临时对象的  构造/析构带来的开销  https://www.cnblogs.com/zhangkele/p/7781593.html
什么是右值呢？简单的左值和右值的判断就是  看是否可以取得地址   可取得地址 是左值     不能则  是右值！
返回值优化、完美转发 就依赖右边值实现...... 什么是返回值优化? 什么是完美转发...?

完美转发
所谓转发，就是通过一个函数将参数继续转交给另一个函数进行处理，原参数可能是右值，可能是左值，如果还能继续保持参数的原有特征，那么它就是完美的。

mysql八股文

由于 InnoDB 是索引组织表，一般情况下我会建议你创建一个自增主键，这样非主键索引占用的空间最小。为什么呢？看即刻时间...



问题：
  a 服务调用 b 服务 超时时间和重试策略 是如何解决的？  
  根据压测 服务可用性 要求几个9  获得的经验值......


//======================redis 八股文===============================



缓存异常的三个问题，分别是 缓存雪崩、缓存击穿、缓存穿透。
这三个问题一旦发生，会导致大量的请求积压到数据库层。
如果请求的并发量很大，就会导致数据库宕机或是故障，这就是很严重的生产事故了。
xjc---雪崩 击穿 穿透 

1 缓存雪崩
缓存雪崩是指大量的应用请求无法在 Redis 缓存中进行处理，紧接着，应用将大量请求发送到数据库层，导致数据库层的压力激增。

第一个种情况：缓存中有大量数据同时过期，导致大量请求无法得到处理。

    除了微调过期时间(就是key失效要随机点不要在同一个时间)，我们还可以通过服务降级，来应对缓存雪崩。

    所谓的服务降级，是指发生缓存雪崩时，针对不同的数据采取不同的处理方式。

      当业务应用访问的是非核心数据（例如电商商品属性）时，暂时停止从缓存中查询这些数据，而是直接返回预定义信息、空值或是错误信息；
      当业务应用访问的是核心数据（例如电商商品库存）时，仍然允许查询缓存，如果缓存缺失，也可以继续通过数据库读取。


第二个种情况:也会发生缓存雪崩，就是，Redis 缓存实例发生故障宕机了，无法处理请求，
            这就会导致大量请求一下子积压到数据库层，从而发生缓存雪崩。

     第一个建议，在业务系统中实现  缓存 服务熔断(就是业务请求缓存的时候直接返回) 
     或  缓存 请求限流机制(就是指，在业务系统的请求入口前端控制每秒进入系统的请求数)。

第三种情况:应用刚刚启动 开会缓存中是没有数据的 都是要去DB中获的数据 (一般rpc 启动都有个并发限制的 这样的情况很少)


2 缓存击穿
缓存击穿是指，针对某个访问非常频繁的热点数据的请求，无法在缓存中进行处理，紧接着，访问该数据的大量请求，
一下子都发送到了后端数据库，导致了数据库压力激增，会影响数据库处理其他请求。缓存击穿的情况，经常发生在热点数据过期失效时，



3 缓存穿透  posid 失效发生过一次....
缓存穿透是指要访问的数据既不在 Redis 缓存中，也不在数据库中，导致请求在访问缓存时，发生缓存缺失，再去访问数据库时，
发现数据库中也没有要访问的数据。此时，应用也无法从数据库中读取数据再写入缓存，来服务后续请求，这样一来，缓存也就成了“摆设”，
如果应用持续有大量请求访问数据，就会同时给缓存和数据库带来巨大压力，


那么，缓存穿透会发生在什么时候呢？一般来说，有两种情况。
业务层误操作：缓存中的数据和数据库中的数据 被误删除了，所以缓存和数据库中都没有数据；
恶意攻击：专门访问数据库中没有的数据。

总结::
从问题成因来看，缓存雪崩和击穿主要是因为数据不在缓存中了，而缓存穿透则是因为数据既不在缓存中，也不在数据库中。
所以，缓存雪崩或击穿时，一旦数据库中的数据被再次写入到缓存后，应用又可以在缓存中快速访问数据了，
数据库的压力也会相应地降低下来，而缓存穿透发生时，Redis 缓存和数据库会同时持续承受请求压力





===================================================
..........缓存系统的设计........  总结........
https://mp.weixin.qq.com/s/qQn8ebq0vv7pT9wemMlF7w

..........redis 锁..........总结............这篇文章要多看几遍.......
https://mp.weixin.qq.com/s/s8xjm1ZCKIoTGT3DCVA4aw

===================================================




===================================================
..........分布式系统中常用的缓存方案有哪些？
客户端缓存:页面和浏览器缓存 app缓存 h5缓存
CDN缓存:  数据缓存 还有负载均衡的作用
nginx缓存: 静态资源缓存
server端缓存: 本地缓存、外部缓存redis
数据库缓存: mysql查询缓存
===================================================

----------------------
分布式缓存寻址算法
	哈希算法:
	一致性哈希算法:
	哈希solt:
----------------------




=====================================================================================================
................................如何解决缓存和数据库的一致性?..............讲解出方案.....................

因为数据库和缓存是分开的，无法做到原子性的同时进行数据的修改,可能会出现缓存更新失败,或者数据库更新失败的情况。两者出现不一致的情况影响业务.
 ---------更新数据的几种可能--------
 1.db更新 后更新缓存。缓存更新可能失败 读到旧数据  (还有就是并发的访问缓存的都是旧数据    如果业务允许也是可以的..)
 2.先删除缓存 再更新db 并发现象读取的时候可能还是会读取到旧的数据  
    (高并发的时候可能还是会出现 缓存中没有就去DB读 恰好DB中此时被更新的过程较慢 关键还是读到db中的旧的数据然后又将这些旧的数据更新到缓存中去了)
 3.db更新 后删除缓存. //也存在缓存删除失败的可能，还存在更新DB的过程较慢 此时并发的请求 读取到缓存的旧数据....（这就看并发度大小带来的影响了）


 3中用的是缓存的删除而不是更新呢？删除更加轻量  更新可能更加耗时因为涉及东西更多



下面这个两个都属于方案 2 一类  先干掉缓存..........
延迟双删 ----- 就是先干掉缓存然后更新数据库 然后隔一个间隔时间再次干掉一次缓存......


更新与读取操作进行  异步串行化 
访问串行化
   .先删除缓存 将更新db操作放入队列中
   .从缓存查找不到的操作都进入有序队列
这样的终极方案会带来问题
   .读请求积压,大量超时,带来db访问的压力:限流 熔断 
   .如何避免大量请求积压:  ---将队列水平拆分,提高并行度



方案3 有一种方法就是 补偿删除 就是将删除放入队列 后续再次删除....


https://mp.weixin.qq.com/s/4W7vmICGx6a_WX701zxgPQ   缓存和数据库的一致性问题...........
=====================================================================================================





=====================================================================================================
..............................如何保证消息不被重复消费?


           加入中间件 用一个唯一的标示 标记这一个信息
======================================================================================================


===================================================
什么是服务降级、什么是服务熔断? (说白了就是减少服务请求的数量来应对)

降级就是将业务进行分类  对核心业务和非核心业务来进行处理降级 来将我们业务损失降到最小。

熔断就是希望业务系统不被外界大流量或者下游系统异常而拖垮. 这里的下游异常就是挂掉了, 不访问下游直接返回默认值...
(熔断 可以防止被拖垮)


服务限流: 也是减少服务请求  怎么减少 是随机丢弃还是 按照业务限定一些条件......
===================================================


---------------------------------------
redis 高可用方案有哪些？怎么构成的？优缺点？

 1主从
 2哨兵
 3cluster 去中心化
 4codis代理
 5tw代理
----------------------------------------


=====================
索引切换双buffer 原理 啥的要..... coding bugfree .....

=====================

自己简历中的项目要熟悉每一处细节  有一些要能展开来讲.............



网络 数据结构  操作系统  c++/go语法要扎实     架构设计要合理  



自己实现一个 String类 .......
#include <memory>

class String
{
public:
    String() :String("") {}// 默认构造函数....
    String(const char *);
    String(const String&);// copy construct
    String& operator=(const String&);// 赋值运算符函数

    //移动构造函数
    String(String &&) NOEXCEPT;
    //移动赋值函数
    String& operator=(String&&)NOEXCEPT;

    ~String();

    const char* c_str() const { return elements; }
    size_t size() const { return end - elements; }
    size_t length() const { return end - elements + 1; }

private:
    std::pair<char*, char*> alloc_n_copy(const char*, const char*);
    void range_initializer(const char*, const char*);
    void free();

private:
    char *elements;
    char *end;
    std::allocator<char> alloc;
};

#include "exercise13_44.h"

#include <algorithm>
#include <iostream>

std::pair<char*,char*>
String::alloc_n_copy(const char* b, const char* e)
{
    auto str = alloc.allocate(e - b);//allocate 成员函数的作用是 分配e-b个类型为T的对象的起始地址 //std::allocator<T> 
    return{ str, std::uninitialized_copy(b, e, str) };// uninitialized_copy 返回的是最后的迭代器
}

void String::range_initializer(const char* first, const char* last)
{
    auto newstr = alloc_n_copy(first, last);// newstr 是一个 std::pair<char*,char*> 类型.......
    elements = newstr.first;
    end = newstr.second;
}

String::String(const char* s)// 构造函数实现 
{
    char *s1 = const_cast<char*>(s);
    while (*s1)
    {
        ++s1;
    }
    range_initializer(s, ++s1);
}

String::String(const String& rhs)// copy 构造函数....
{
    range_initializer(rhs.elements, rhs.end);
    std::cout << "copy constructor" << std::endl;
}

//move construc
String::String(String &&s) NOEXCEPT : elements(s.elements), end(s.end)
{
    s.elements = s.end = nullptr;
}

String& String::operator = (String &&rhs) NOEXCEPT
{
    if (this != &rhs) {
        free();
        elements = rhs.elements;
        end = rhs.end;
        rhs.elements = rhs.end = nullptr;
    }
    return *this;
}


void String::free()
{
    if (elements)
    {
        std::for_each(elements, end, [this](char &c) { alloc.destroy(&c); });
        alloc.deallocate(elements, end - elements);
    }
}

String::~String()
{
    free();
}

String& String::operator=(const String& rhs)
{
    auto newstr = alloc_n_copy(rhs.elements, rhs.end);
    free();
    elements = newstr.first;
    end = newstr.second;
    std::cout << "copy-assignment" << std::endl;
    return *this;
}

void foo(String x)
{
    std::cout << x.c_str() << std::endl;
}

void bar(const String& x)
{
    std::cout << x.c_str() << std::endl;
}

String baz()
{
    String ret("world");
    return ret;
}

int main()
{
    char text[] = "world";

    String s0;
    String s1("hello");
    String s2(s0);
    String s3 = s1;
    String s4(text);
    s2 = s1;

    foo(s1);
    bar(s1);
    foo("temporary");
    bar("temporary");
    String s5 = baz();

    std::vector<String> svec;
    svec.reserve(8);
    svec.push_back(s0);
    svec.push_back(s1);
    svec.push_back(s2);
    svec.push_back(s3);
    svec.push_back(s4);
    svec.push_back(s5);
    svec.push_back(baz());
    svec.push_back("good job");

    for (const auto &s : svec)
    {
        std::cout << s.c_str() << std::endl;
    }
}

//高性能线程安全的哈希表 （分桶）
template <typename Key, typename Value>
class ConcurrentHashMap{
private:
	struct Bucket{
		std::list<std::pair<Key,Value>> data;
		matbale std::mutex mutex;
	};

	std::vector<Bucket> buckets_;
	size_t bucket_count_;
	size_t hash(const Key&key) const{
		return std::hash<Key>{}(key)%bucket_count_;
	}

public:
	ConcurrentHashMap(size_t bucket_count = 64):buckets_(bucket_count),bucket_count_(bucket_count){}

	void insert(const Key &key, const Value &value){
		size_t index = hash(key);
		std::lock_guard<std::mutex> lock(buckets_[index].mutex);
		auto& bucket = buckets_[index].data;
		auto it = std::find_id(bucket.begin(),bucket.end(),[&](const auto& pair){
			return pair.first == key;
		});
		if(if != bucket,end()){
			it->seconde = value;
		}else{
			bucket.emplace_back(key,value);
		}
	}

	bool get(cosnt Key&key,Value &value) const{
		size_t index = hash(key);
		std::lock_guard<std::mutex> lock(buckets_[index].mutex);
		const auto&bucket = buckets_[index].data;
		auto it = std::find_if(bucket.begin(),bucket.end(),
		[&](const auto* pair)){
			return pair.first == key;
		});

		if(it != bucket.end()){
			value = it->second;
			return true;
		}
		return false;
	}
};
// 分别对每一个桶加锁 减少力度 减少锁竞争 提升并发度
// list 处理哈希冲突 支持动态扩容



在C++中实现一个简单的DAG（有向无环图）可以通过使用图的表示方式，例如邻接表或邻接矩阵。我们可以使用邻接表来表示图，并实现一些基本的操作，例如添加节点、添加边、检测环等。

下面是一个简单的DAG实现，使用邻接表来表示图，并包含检测环的功能：

### 代码实现：

```cpp
#include <iostream>
#include <list>
#include <vector>
#include <queue>

class DAG {
private:
    int numVertices;  // 图中的顶点数量
    std::vector<std::list<int>> adjList;  // 邻接表

    // DFS辅助函数，用于检测环
    bool isCyclicUtil(int vertex, std::vector<bool>& visited, std::vector<bool>& recursionStack);

public:
    DAG(int vertices);  // 构造函数

    // 添加边到图中
    void addEdge(int src, int dest);

    // 检测图中是否有环
    bool isCyclic();

    // 打印图
    void printGraph();
};

// 构造函数，初始化邻接表
DAG::DAG(int vertices) {
    numVertices = vertices;
    adjList.resize(numVertices);
}

// 添加边的函数
void DAG::addEdge(int src, int dest) {
    if (src >= numVertices || dest >= numVertices) {
        std::cerr << "Error: Vertex index out of bounds." << std::endl;
        return;
    }
    adjList[src].push_back(dest);  // 添加边到邻接表
}

// 检测环的DFS辅助函数
bool DAG::isCyclicUtil(int vertex, std::vector<bool>& visited, std::vector<bool>& recursionStack) {
    // 标记当前节点为已访问
    visited[vertex] = true;
    recursionStack[vertex] = true;

    // 遍历当前节点的所有邻接节点
    for (auto it = adjList[vertex].begin(); it != adjList[vertex].end(); ++it) {
        int adjVertex = *it;

        // 如果邻接节点没有被访问，则递归检查
        if (!visited[adjVertex]) {
            if (isCyclicUtil(adjVertex, visited, recursionStack))
                return true;
        }
        // 如果邻接节点已经在递归栈中，则说明有环
        else if (recursionStack[adjVertex]) {
            return true;
        }
    }

    // 当前节点的递归栈标记为false
    recursionStack[vertex] = false;
    return false;
}

// 检测图中是否有环
bool DAG::isCyclic() {
    std::vector<bool> visited(numVertices, false);        // 访问标记
    std::vector<bool> recursionStack(numVertices, false); // 递归栈标记

    // 遍历所有顶点
    for (int i = 0; i < numVertices; ++i) {
        if (!visited[i]) {
            if (isCyclicUtil(i, visited, recursionStack))
                return true;
        }
    }

    return false;
}

// 打印图的邻接表
void DAG::printGraph() {
    for (int i = 0; i < numVertices; ++i) {
        std::cout << "Adjacency list of vertex " << i << ": ";
        for (auto it = adjList[i].begin(); it != adjList[i].end(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;
    }
}

int main() {
    DAG dag(4);  // 创建一个包含4个顶点的DAG

    // 添加边
    dag.addEdge(0, 1);
    dag.addEdge(0, 2);
    dag.addEdge(1, 2);
    dag.addEdge(2, 3);

    // 打印图
    dag.printGraph();

    // 检测是否有环
    if (dag.isCyclic()) {
        std::cout << "The graph contains a cycle." << std::endl;
    } else {
        std::cout << "The graph does not contain a cycle." << std::endl;
    }

    return 0;
}
```

### 代码说明：

1. **DAG类**：这是一个简单的DAG类，使用邻接表来表示图。邻接表是一个 `std::vector<std::list<int>>`，其中每个顶点都有一个邻接列表。

2. **addEdge函数**：用于在图中添加边。注意DAG是有向图，所以只从源节点指向目标节点。

3. **isCyclic函数**：用于检测图中是否有环。它通过DFS（深度优先搜索）算法来检查是否存在环。

4. **isCyclicUtil函数**：这是一个递归函数，用于辅助检测环。它使用 `visited` 数组来标记已经访问过的节点，使用 `recursionStack` 数组来标记当前递归路径中的节点。如果某个节点已经在递归栈中被访问过，则说明存在环。

5. **printGraph函数**：用于打印图的邻接表。

### 运行结果：

假设我们添加的边是 `0 -> 1`, `0 -> 2`, `1 -> 2`, `2 -> 3`，那么图的邻接表如下：

```
Adjacency list of vertex 0: 1 2 
Adjacency list of vertex 1: 2 
Adjacency list of vertex 2: 3 
Adjacency list of vertex 3: 
```

由于这是一个DAG，没有环，所以输出为：

```
The graph does not contain a cycle.
```

### 扩展：

你可以通过修改 `addEdge` 函数来添加更多的边，甚至可以尝试添加一个环（例如 `3 -> 0`），看看检测环的功能是否正常工作。

例如：
```cpp
dag.addEdge(3, 0);
```

这样就会形成一个环，检测结果会变成：

```
The graph contains a cycle.
```

### 总结：

这个实现是一个简单的DAG类，能够添加边、检测环并打印图的结构。你可以根据需求扩展它，例如添加拓扑排序功能、删除边、检查节点是否存在等。







