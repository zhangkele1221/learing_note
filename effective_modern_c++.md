# 条款一：理解模板类型推导
可以考虑像这样一个函数模板：
```cpp
template<typename T>
void f(ParamType param);

f(expr);    //使用表达式调用f
```
在编译期间，编译器使用expr进行两个类型推导： <font color = red>一个是针对T的，另一个是针对ParamType的。</font>  <font color = green>这两个类型通常是不同的，因为ParamType包含一些修饰，比如const和引用修饰符。</font>举个例子，如果模板这样声明：

```cpp
template<typename T>
void f(const T& param);   //ParamType是const T&

//调用
int x = 0;
f(x);                    //用一个int类型的变量调用f

T被推导为int，ParamType却被推导为const int&


但有时情况并非总是如此，T的类型推导不仅取决于expr的类型，也取决于ParamType的类型。

有三种情况：


1.ParamType是一个指针或引用，但不是通用引用（通用引用参见24条 需要知道它存在，且不同于左值引用和右值引用）
2.ParamType一个通用引用
3.ParamType既不是指针也不是引用
```
分成三个情景来讨论这三种情况，每个情景的都基于我们之前给出的模板：
```cpp
template<typename T>
void f(ParamType param);

f(expr);     //从expr中推导T和ParamType
```


<font color = red> 情景一：ParamType是一个指针或引用，但不是通用引用  </font>
```cpp
最简单的情况是ParamType是一个指针或者引用，但非通用引用

1.如果expr的类型是一个引用，忽略引用部分
2.然后expr的类型与ParamType进行模式匹配来决定T

template<typename T>
void f(T& param);               //param是一个引用

int x=27;                       //x是int
const int cx=x;                 //cx是const int
const int& rx=x;                //rx是指向作为const int的x的引用

f(x);                           //T是int，param的类型是int&
f(cx);                          //T是const int，param的类型是const int&
f(rx);                          //T是const int，param的类型是const int&

//第三个例子中，注意即使rx的类型是一个引用，T也会被推导为一个非引用 ，这是因为rx的引用性（reference-ness）在类型推导中会被忽略。

template<typename T>
void f(const T& param);         //param现在是reference-to-const

int x = 27;                     //如之前一样
const int cx = x;               //如之前一样
const int& rx = x;              //如之前一样

f(x);                           //T是int，param的类型是const int&
f(cx);                          //T是int，param的类型是const int&
f(rx);                          //T是int，param的类型是const int&

//同上 rx的引用性（reference-ness）在类型推导中会被忽略。

如果param是一个指针（或者指向const的指针）而不是引用，情况本质上也一样：

template<typename T>
void f(T* param);               //param现在是指针

int x = 27;                     //同之前一样
const int *px = &x;             //px是指向作为const int的x的指针

f(&x);                          //T是int，param的类型是int*
f(px);                          //T是const int，param的类型是const int*


```


<font color = red>情景二：ParamType是一个通用引用  </font>
```cpp
/*首先要明白 ParamType是一个通用引用 什么时候才是通用引用 切记why*/
模板使用通用引用形参的话，那事情就不那么明显了。这样的形参被声明为像右值引用一样（也就是，在函数模板中假设有一个类型形参T，那么通用引用声明形式就是T&&)，它们的行为在传入左值实参时大不相同。完整的叙述请参见Item24，在这有些最必要的你还是需要知道：看了24条款就明白了 通用引用类型推导过程了

1.如果expr是左值，T和ParamType都会被推导为左值引用。这非常不寻常，第一，这是模板类型推导中唯一一种T被推导为引用的情况。第二，虽然ParamType被声明为右值引用类型，但是最后推导的结果是左值引用。自己理解吧 不清楚就看24条款

2.如果expr是右值，就使用正常的（也就是情景一）推导规则


template<typename T>
void f(T&& param);              //param现在是一个通用引用类型
        
int x=27;                       //如之前一样
const int cx=x;                 //如之前一样
const int & rx=cx;              //如之前一样

f(x);                           //x是左值，所以T是int&，
                                //param类型也是int&

f(cx);                          //cx是左值，所以T是const int&，
                                //param类型也是const int&

f(rx);                          //rx是左值，所以T是const int&，
                                //param类型也是const int&

f(27);                          //27是右值，所以T是int，
                                //param类型就是int&&

Item24详细解释了为什么这些例子是像这样发生的。/*这里关键在于通用引用的类型推导规则是不同于普通的左值或者右值引用的。尤其是，当通用引用被使用时，类型推导会区分左值实参和右值实参，但是对非通用引用时不会区分。*/

```
<font color = red>情景三： ParamType既不是指针也不是引用  </font>
```cpp
当ParamType既不是指针也不是引用时，我们通过传值（pass-by-value）的方式处理：
template<typename T>
void f(T param);                //以传值的方式处理param

//要理解哦！！！！
1.和之前一样，如果expr的类型是一个引用，忽略这个引用部分
2.如果忽略expr的引用性（reference-ness）之后，expr是一个const，那就再忽略const。如果它是volatile，也忽略volatile（volatile对象不常见，它通常用于驱动程序的开发中。关于volatile的细节请参见Item40）

int x=27;                       //如之前一样
const int cx=x;                 //如之前一样
const int & rx=cx;              //如之前一样

f(x);                           //T和param的类型都是int
f(cx);                          //T和param的类型都是int
f(rx);                          //T和param的类型都是int

```

<font color = red>
记住：

1.在模板类型推导时，有引用的实参会被视为无引用，他们的引用会被忽略

2.对于通用引用的推导，左值实参会被特殊对待

3.对于传值类型推导，const和/或volatile实参会被认为是non-const的和non-volatile的

4.在模板类型推导时，数组名或者函数名实参会退化为指针，除非它们被用于初始化引用
 </font>

<br />
<br />

# 条款24：理解模板类型推导