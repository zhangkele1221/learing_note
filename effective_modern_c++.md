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
https://cntransgroup.github.io/EffectiveModernCppChinese/5.RRefMovSemPerfForw/item24.html

为了声明一个指向某个类型T的右值引用，你写下了T&&。由此，一个合理的假设是，当你看到一个“T&&”出现在源码中，你看到的是一个右值引用。唉，事情并不如此简单:
```cpp
void f(Widget&& param);             //右值引用
Widget&& var1 = Widget();           //右值引用

auto&& var2 = var1;                 //不是右值引用 why!!!!!!!!!

template<typename T>
void f(std::vector<T>&& param);     //右值引用

template<typename T>
void f(T&& param);                  //不是右值引用 why!!!!!!!!

```
T&&”有两种不同的意思。第一种，当然是右值引用。这种引用表现得正如你所期待的那样：它们只绑定到右值上，并且它们主要的存在原因就是为了识别可以移动操作的对象。

“T&&”的另一种意思是，它既可以是右值引用，也可以是左值引用。这种引用在源码里看起来像右值引用（即“T&&”），但是它们可以表现得像是左值引用（即“T&”）。它们的二重性使它们既可以绑定到右值上（就像右值引用），也可以绑定到左值上（就像左值引用）。 此外，它们还可以绑定到const或者non-const的对象上，也可以绑定到volatile或者non-volatile的对象上，甚至可以绑定到既const又volatile的对象上。它们可以绑定到几乎任何东西。这种空前灵活的引用值得拥有自己的名字。我把它叫做通用引用（universal references）。（Item25解释了std::forward几乎总是可以应用到通用引用上，并且在这本书即将出版之际，一些C++社区的成员已经开始将这种通用引用称之为转发引用（forwarding references））。

<font color = red>
在两种情况下会出现通用引用。最常见的一种是函数模板形参，正如在之前的示例代码中所出现的例子：
</font>

```cpp
template<typename T>
void f(T&& param);                  //param是一个通用引用

//第二种情况是auto声明符
auto&& var2 = var1;                 //var2是一个通用引用
```
<font color = red>这两种情况的共同之处就是都存在类型推导。</font>
在模板f的内部，param的类型需要被推导，而在变量var2的声明中，var2的类型也需要被推导。同以下的例子相比较（同样来自于上面的示例代码），下面的例子不带有类型推导。<font color = red>如果你看见“T&&”不带有类型推导，那么你看到的就是一个右值引用：</font>

```c++
void f(Widget&& param);         //没有类型推导，
                                //param是一个右值引用

Widget&& var1 = Widget();       //没有类型推导，
                                //var1是一个右值引用
```

------------------
------------------
.................

<font color = red>
记住：

1.如果一个函数模板形参的类型为T&&，并且T需要被推导得知，或者如果一个对象被声明为auto&&，这个形参或者对象就是一个通用引用。

2.如果类型声明的形式不是标准的type&&，或者如果类型推导没有发生，那么type&&代表一个右值引用。

3.通用引用，如果它被右值初始化，就会对应地成为右值引用；如果它被左值初始化，就会成为左值引用。
</font>

----------------------------
<br />
<br />


# 二十五：对右值引用使用std::move，对通用引用使用std::forward
有经验的自己看了标题就能懂了 就不要向下看了

```cpp





```


<font color = red>

请记住：且更要理解其中原理！！！！！！

1.最后一次使用时，在右值引用上使用std::move，在通用引用上使用std::forward。

2.对按值返回的函数要返回的右值引用和通用引用，执行相同的操作。

3.如果局部对象可以被返回值优化消除，就绝不使用std::move或者std::forward。
</font>


<br />
<br />

# 5章 右值引用，移动语义，完美转发

移动语义使编译器有可能用廉价的移动操作来代替昂贵的拷贝操作。正如拷贝构造函数和拷贝赋值操作符给了你控制拷贝语义的权力，移动构造函数和移动赋值操作符也给了你控制移动语义的权力。移动语义也允许创建只可移动的类型，如std::unique_ptr，std::future和std::thread。

完美转发使接收任意数量实参的函数模板成为可能，它可以将实参转发到其他的函数，使目标函数接收到的实参与被传递给转发函数的实参保持一致。

右值引用是连接这两个截然不同的概念的胶合剂。它是使移动语义和完美转发变得可能的基础语言机制。

本章的这些小节中，非常重要的一点是要牢记 <font color = red>形参永远是左值，即使它的类型是一个右值引用。</font>  如下所示
```cpp
void f(Widget&& w);

//形参w是一个左值，即使它的类型是一个 右值引用的样子
```



## 条款23 理解std::move 和std::forward

直接看std::move 源代码实现
```cpp
template<typename T>                            //在std命名空间
typename remove_reference<T>::type&&
move(T&& param)
{
    using ReturnType =                          //别名声明，见条款9
        typename remove_reference<T>::type&&;

    return static_cast<ReturnType>(param);
}

//正如你所见，std::move接受一个对象的引用（准确的说，一个通用引用（universal reference），见Item24)，返回一个指向同对象的引用。


template<typename T>
decltype(auto) move(T&& param)          //C++14，仍然在std命名空间
{
    using ReturnType = remove_referece_t<T>&&;
    return static_cast<ReturnType>(param);
}

```
对比下面的代码
```c++
class Annotation {
public:
    explicit Annotation(std::string text);  //将会被复制的形参，
    …                                       //如同条款41所说，
};                                          //值传递

//但是Annotation类的构造函数仅仅是需要读取text的值，它并不需要修改它。为了和历史悠久的传统：能使用const就使用const保持一致，你修订了你的声明以使text变成const：

class Annotation {
public:
    explicit Annotation(const std::string text);
    …
};

//当复制text到一个数据成员的时候，为了避免一次复制操作的代价，你仍然记得来自Item41的建议，把std::move应用到text上，因此产生一个右值：

class Annotation {
public:
    explicit Annotation(const std::string text)
    ：value(std::move(text))    //“移动”text到value里；这段代码执行起来
    { … }                       //并不是看起来那样  为什么呢？???????? 继续往下看
    
    …

private:
    std::string value;
};

//这段代码可以编译，可以链接，可以运行。这段代码将数据成员value设置为text的值。这段代码与你期望中的完美实现的唯一区别，是text并不是被移动到value，而是被拷贝  为什么呢？？？？？？？


//诚然，text通过std::move被转换到右值，但是text被声明为const std::string，所以在转换之前，text是一个左值的const std::string，而转换的结果是一个右值的const std::string，但是纵观全程，const属性一直保留。


当编译器决定哪一个std::string的构造函数被调用时，考虑它的作用，将会有两种可能性：

class string {                  //std::string事实上是
public:                         //std::basic_string<char>的类型别名
    …
    string(const string& rhs);  //拷贝构造函数
    string(string&& rhs);       //移动构造函数
    …
};



```
<font color = red>
在类Annotation的构造函数的成员初始化列表中，std::move(text)的结果是一个const std::string的右值。这个右值不能被传递给std::string的移动构造函数，因为移动构造函数只接受一个指向non-const的std::string的右值引用。然而，该右值却可以被传递给std::string的拷贝构造函数，因为lvalue-reference-to-const允许被绑定到一个const右值上。
因此，std::string在成员初始化的过程中调用了拷贝构造函数，即使text已经被转换成了右值。
这样是为了确保维持const属性的正确性。从一个对象中移动出某个值通常代表着修改该对象，所以语言不允许const对象被传递给可以修改他们的函数（例如移动构造函数）</font>

<br />

从这个例子，总结出两点。

第一，不要在你希望能移动对象的时候，声明他们为const。对const对象的移动请求会悄无声息的被转化为拷贝操作

第二，std::move不仅不移动任何东西，而且它也不保证它执行转换的对象可以被移动。

关于std::move，你能确保的唯一一件事就是将它应用到一个对象上，你能够得到一个右值。