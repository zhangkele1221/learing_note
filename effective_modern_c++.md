# 条款一：理解模板类型推导

可以考虑像这样一个函数模板：

```cpp
template<typename T>
void f(ParamType param);

f(expr);    //使用表达式调用f
```

在编译期间，编译器使用expr进行两个类型推导： <font color = red>一个是针对T的，另一个是针对ParamType的。</font>  <font color = green>这两个类型通常是不同的，因为ParamType包含一些修饰，比如const和引用修饰符。</font>`举个例子，如果模板这样声明：

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

# 条款24：理解模板类型推导

https://cntransgroup.github.io/EffectiveModernCppChinese/5.RRefMovSemPerfForw/item24.html

为了声明一个指向某个类型T的右值引用，你写下了T&&。由此，一个合理的假设是，当你看到一个“T&&”出现在源码中，你看到的是一个右值引用。唉，事情并不如此简单:

T&&”有两种不同的意思。第一种，当然是右值引用。这种引用表现得正如你所期待的那样：它们只绑定到右值上，并且它们主要的存在原因就是为了识别可以移动操作的对象。

“T&&”的另一种意思是，它既可以是右值引用，也可以是左值引用。这种引用在源码里看起来像右值引用（即“T&&”），但是它们可以表现得像是左值引用（即“T&”）。它们的二重性使它们既可以绑定到右值上（就像右值引用），也可以绑定到左值上（就像左值引用）。 此外，它们还可以绑定到const或者non-const的对象上，也可以绑定到volatile或者non-volatile的对象上，甚至可以绑定到既const又volatile的对象上。它们可以绑定到几乎任何东西。这种空前灵活的引用值得拥有自己的名字。我把它叫做通用引用（universal references）。（Item25解释了std::forward几乎总是可以应用到通用引用上，并且在这本书即将出版之际，一些C++社区的成员已经开始将这种通用引用称之为转发引用（forwarding references））。

<font color = red>这两种情况的共同之处就是都存在类型推导。</font>
在模板f的内部，param的类型需要被推导，而在变量var2的声明中，var2的类型也需要被推导。同以下的例子相比较（同样来自于上面的示例代码），下面的例子不带有类型推导。<font color = red>如果你看见“T&&”不带有类型推导，那么你看到的就是一个右值引用：</font>

---

---

.................

1.如果一个函数模板形参的类型为T&&，并且T需要被推导得知，或者如果一个对象被声明为auto&&，这个形参或者对象就是一个通用引用。

2.如果类型声明的形式不是标准的type&&，或者如果类型推导没有发生，那么type&&代表一个右值引用。

3.通用引用，如果它被右值初始化，就会对应地成为右值引用；如果它被左值初始化，就会成为左值引用。
</font>

---

# 二十五：对右值引用使用std::move，对通用引用使用std::forward

有经验的自己看了标题就能懂了 就不要向下看了

请记住：且更要理解其中原理！！！！！！

1.最后一次使用时，在右值引用上使用std::move，在通用引用上使用std::forward。

2.对按值返回的函数要返回的右值引用和通用引用，执行相同的操作。

3.如果局部对象可以被返回值优化消除，就绝不使用std::move或者std::forward。
</font>

# 5章 右值引用，移动语义，完美转发

移动语义使编译器有可能用廉价的移动操作来代替昂贵的拷贝操作。正如拷贝构造函数和拷贝赋值操作符给了你控制拷贝语义的权力，移动构造函数和移动赋值操作符也给了你控制移动语义的权力。移动语义也允许创建只可移动的类型，如std::unique_ptr，std::future和std::thread。

完美转发使接收任意数量实参的函数模板成为可能，它可以将实参转发到其他的函数，使目标函数接收到的实参与被传递给转发函数的实参保持一致。

右值引用是连接这两个截然不同的概念的胶合剂。它是使移动语义和完美转发变得可能的基础语言机制。

本章的这些小节中，非常重要的一点是要牢记 <font color = red>形参永远是左值，即使它的类型是一个右值引用。</font>  如下所示

## 条款23 理解std::move 和std::forward

直接看std::move 源代码实现

对比下面的代码

 ............这里没有提交成功 下次重新补上...........

从这个例子，总结出两点。

<font color = green>
第一，不要在你希望能移动对象的时候，声明他们为const。对const对象的移动请求会悄无声息的被转化为拷贝操作

第二，std::move不仅不移动任何东西，而且它也不保证它执行转换的对象可以被移动。</font> 

关于std::move，你能确保的唯一一件事就是将它应用到一个对象上，你能够得到一个右值。

----

关于std::forward的故事与std::move是相似的，但是与std::move总是无条件的将它的实参为右值不同，std::forward只有在满足一定条件的情况下才执行转换。std::forward是有条件的转换。要明白什么时候它执行转换，什么时候不，想想std::forward的典型用法。最常见的情景是一个模板函数，接收一个通用引用形参，并将它传递给另外的函数：

```cpp
void process(const Widget& lvalArg);        //处理左值
void process(Widget&& rvalArg);             //处理右值


template<typename T>                        //用以转发param到process的模板
void logAndProcess(T&& param)
{
    ....

    process(std::forward<T>(param)); //这就叫完美转发哦！！！！！

    // process(param); 这里如果写成这样的调用 就会调用  process(const Widget& lvalArg); 左值调用的处理版本了 
    ....
}

Widget w;

logAndProcess(w);               //用左值调用
logAndProcess(std::move(w));    //用右值调用


但是param，正如所有的其他函数形参一样，是一个左值。每次在函数logAndProcess内部对函数process的调用，都会因此调用函数process的左值重载版本。为防如此，我们需要一种机制：当且仅当传递给函数logAndProcess的用以初始化param的实参是一个右值时，param会被转换为一个右值。这就是std::forward做的事情。这就是为什么std::forward是一个有条件的转换：它的实参用右值初始化时，转换为一个右值。

你也许会想知道std::forward是怎么知道它的实参是否是被一个右值初始化的。举个例子，在上述代码中，std::forward是怎么分辨param是被一个左值还是右值初始化的？ 简短的说，该信息藏在函数logAndProcess的模板参数T中。该参数被传递给了函数std::forward，它解开了含在其中的信息。该机制工作的细节可以查询Item28。

```
---
考虑到std::move和std::forward都可以归结于转换，它们唯一的区别就是std::move总是执行转换，而std::forward偶尔为之。你可能会问是否我们可以免于使用std::move而在任何地方只使用std::forward。 从纯技术的角度，答案是yes：std::forward是可以完全胜任，std::move并非必须.
对比如下 两个例子
```cpp
class Widget {
public:
    Widget(Widget&& rhs)
    : s(std::move(rhs.s))
    { ++moveCtorCalls; }

    …

private:
    static std::size_t moveCtorCalls;
    std::string s;
};


class Widget{
public:
    Widget(Widget&& rhs)                    //不自然，不合理的实现
    : s(std::forward<std::string>(rhs.s))
    { ++moveCtorCalls; }

    …

}

//看完这两个有什么想法 或者思考 不懂就看书

```

<font color = red>
记住：

1.std::move执行到右值的无条件的转换，但就自身而言，它不移动任何东西。

2.std::forward只有当它的参数被绑定到一个右值时，才将参数转换为右值。

3.std::move和std::forward在运行期什么也不做。</font>

---------------------------------------
=======================================

# 条款22 使用pimpl  在实现文件中定义特殊成员函数

看代码
```cpp

class Widget() {                    //定义在头文件“widget.h”
public:
    Widget();
    …
private:
    std::string name;
    std::vector<double> data;
    Gadget g1, g2, g3;              //Gadget是用户自定义的类型
};

//这些头文件将会增加类Widget使用者的编译时间，并且让这些使用者依赖于这些头文件。 如果一个头文件的内容变了，类Widget使用者也必须要重新编译。 标准库文件<string>和<vector>不是很常变，但是gadget.h可能会经常修订。


在C++98中使用Pimpl惯用法，可以把Widget的数据成员替换成一个原始指针，指向一个已经被声明过却还未被定义的结构体，如下:

class Widget                        //仍然在“widget.h”中
{
public:
    Widget();
    ~Widget();                      //析构函数在后面会分析
    …

private:
    struct Impl;                    //声明一个 实现结构体
    Impl *pImpl;                    //以及指向它的指针
};

因为类Widget不再提到类型std::string，std::vector以及Gadget，Widget的使用者不再需要为了这些类型而引入头文件。 这可以加速编译，//并且意味着，如果这些头文件中有所变动，Widget的使用者不会受到影响。


```

一个已经被声明，却还未被实现的类型，被称为未完成类型（incomplete type）。 Widget::Impl就是这种类型。 <font color = red>你能对一个未完成类型做的事很少，但是声明一个指向它的指针是可以的.</font> Pimpl惯用法利用了这一点。

```cpp
Pimpl惯用法的第一步，是声明一个数据成员，它是个指针，指向一个未完成类型。 第二步是动态分配和回收一个对象，该对象包含那些以前在原来的类中的数据成员。 内存分配和回收的代码都写在实现文件里，比如，对于类Widget而言，写在Widget.cpp里:


//以下代码均在实现文件“widget.cpp”里

//c++ 98 的写法
#include "widget.h"             
#include "gadget.h"
#include <string>
#include <vector>

struct Widget::Impl {           //含有之前在Widget中的数据成员的
    std::string name;           //Widget::Impl类型的定义
    std::vector<double> data;
    Gadget g1,g2,g3;
};

Widget::Widget()                //为此Widget对象分配数据成员
: pImpl(new Impl)
{}

Widget::~Widget()               //销毁数据成员
{ delete pImpl; }

```
<font color = red>
上面把#include命令写出来是为了明确一点，对于std::string，std::vector和Gadget的头文件的整体依赖依然存在。
然而，<font color = yellow>这些依赖从头文件widget.h（它被所有Widget类的使用者包含，并且对他们可见）移动到了widget.cpp</font> （该文件只被Widget类的实现者包含，并只对他可见). </font>

-------

原始的new和原始的delete，一切都让它如此的...原始。这一章建立在“智能指针比原始指针更好”的主题上，并且，如果我们想要的只是在类Widget的构造函数动态分配Widget::impl对象，在Widget对象销毁时一并销毁它， std::unique_ptr（见Item18）是最合适的工具。在头文件中用std::unique_ptr替代原始指针，就有了头文件中如下代码:

```c++
//c++11 写法

//在“widget.h”中
class Widget {         
public:
    Widget();
    …

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;    //使用智能指针而不是原始指针
};

//在“widget.cpp”中
#include "widget.h"               
#include "gadget.h"
#include <string>
#include <vector>

struct Widget::Impl {               //跟之前一样
    std::string name;
    std::vector<double> data;
    Gadget g1,g2,g3;
};

Widget::Widget()                    //根据条款21，通过std::make_unique
: pImpl(std::make_unique<Impl>())   //来创建std::unique_ptr
{}



```
上面的还没有写完!!!!!!!!!!!!!!!!


--------------------------------
# 条款17 理解特殊成员函数的生成

特殊成员函数是指C++自己生成的函数。<font color = brew> C++98有四个：1.默认构造函数  2.析构函数  3.拷贝构造函数  4.拷贝赋值运算符  </font> 当然在这里有些细则要注意。<font color = red>这些函数仅在需要的时候才生成，比如某个代码使用它们但是它们没有在类中明确声明。</font> ----- <font color = orange>默认构造函数仅在类完全没有构造函数的时候才生成。</font>（防止编译器为某个类生成构造函数，但是你希望那个构造函数有参数）生成的特殊成员函数是隐式public且inline，它们是非虚的，除非相关函数是在派生类中的析构函数，派生类继承了有虚析构函数的基类。在这种情况下，编译器为派生类生成的析构函数是虚的。

C++11特殊成员函数俱乐部迎来了两位新会员：<font color = green>移动构造函数和移动赋值运算符。</font>

```cpp
class Widget {
public:
    …
    Widget(Widget&& rhs);               //移动构造函数
    Widget& operator=(Widget&& rhs);    //移动赋值运算符
    …
};

掌控它们生成和行为的规则类似于拷贝系列。
移动操作仅在需要的时候生成，如果生成了，就会对类的non-static数据成员执行逐成员的移动。
那意味着移动构造函数根据rhs参数里面对应的成员移动构造出新non-static部分，移动赋值运算符根据参数里面对应的non-static成员移动赋值。
移动构造函数也移动构造基类部分（如果有的话），移动赋值运算符也是移动赋值基类部分。


当我对一个数据成员或者基类使用移动构造或者移动赋值时，没有任何保证移动一定会真的发生。
逐成员移动，实际上，更像是逐成员移动请求，因为对不可移动类型（即对移动操作没有特殊支持的类型，比如大部分C++98传统类）使用“移动”操作实际上执行的是拷贝操作。
逐成员移动的核心是对对象使用std::move，然后函数决议时会选择执行移动还是拷贝操作。Item23包括了这个操作的细节。本条款中，简单记住如果支持移动就会逐成员移动类成员和基类成员，如果不支持移动就执行拷贝操作就好了。


像拷贝操作情况一样，如果你自己声明了移动操作，编译器就不会生成。
然而它们生成的精确条件与拷贝操作的条件有点不同。


```

---------
<font color = brew>
两个拷贝操作是独立的：声明一个不会限制编译器生成另一个。
所以如果你声明一个拷贝构造函数，但是没有声明拷贝赋值运算符，如果写的代码用到了拷贝赋值，编译器会帮助你生成拷贝赋值运算符。
同样的，如果你声明拷贝赋值运算符但是没有拷贝构造函数，代码用到拷贝构造函数时编译器就会生成它。上述规则在C++98和C++11中都成立。
</font>

-------
<font color = green>
但是,
两个移动操作不是相互独立的。如果你声明了其中一个，编译器就不再生成另一个。如果你给类声明了，比如，一个移动构造函数，就表明对于移动操作应怎样实现，与编译器应生成的默认逐成员移动有些区别。如果逐成员移动构造有些问题，那么逐成员移动赋值同样也可能有问题。

所以声明移动构造函数阻止移动赋值运算符的生成，声明移动赋值运算符同样阻止编译器生成移动构造函数。
</font>

<br/>
进一步讲解: <font color = red>如果一个类显式声明了拷贝操作，编译器就不会生成移动操作。</font>
这种限制的解释是如果声明拷贝操作（构造或者赋值）就暗示着平常拷贝对象的方法（逐成员拷贝）不适用于该类，编译器会明白如果逐成员拷贝对拷贝操作来说不合适，逐成员移动也可能对移动操作来说不合适。

<br/>
另一个方向,  <font color = red>声明移动操作（构造或赋值）使得编译器禁用拷贝操作。（编译器通过给拷贝操作加上delete来保证，参见Item11。）（译注：禁用的是自动生成的拷贝操作，对于用户声明的拷贝操作不受影响）</font> 毕竟，如果逐成员移动对该类来说不合适，也没有理由指望逐成员拷贝操作是合适的。听起来会破坏C++98的某些代码，因为C++11中拷贝操作可用的条件比C++98更受限，但事实并非如此。C++98的代码没有移动操作，因为C++98中没有移动对象这种概念。只有一种方法能让老代码使用用户声明的移动操作，那就是使用C++11标准然后添加这些操作，使用了移动语义的类必须接受C++11特殊成员函数生成规则的限制。



<br/>
Rule of Three_规则。规则告诉我们  <font color = red> 如果你声明了拷贝构造函数，拷贝赋值运算符，或者析构函数三者之一，你应该也声明其余两个。</font> 它来源于长期的观察，即用户接管拷贝操作的需求几乎都是因为该类会做其他资源的管理，这也几乎意味着（1）无论哪种资源管理如果在一个拷贝操作内完成，也应该在另一个拷贝操作内完成（2）类的析构函数也需要参与资源的管理（通常是释放）。通常要管理的资源是内存，这也是为什么标准库里面那些管理内存的类（如会动态内存管理的STL容器）都声明了“the big three”：拷贝构造，拷贝赋值和析构。

Rule of Three带来的后果就是只要出现用户定义的析构函数就意味着简单的逐成员拷贝操作不适用于该类。<font color = green>那意味着如果一个类声明了析构，拷贝操作可能不应该自动生成，因为它们做的事情可能是错误的。</font> 在C++98提出的时候，上述推理没有得倒足够的重视，所以C++98用户声明析构函数不会左右编译器生成拷贝操作的意愿。C++11中情况仍然如此，但仅仅是因为限制拷贝操作生成的条件会破坏老代码。

Rule of Three规则背后的解释依然有效，再加上对声明拷贝操作阻止移动操作隐式生成的观察，<font color = red>使得C++11不会为那些有用户定义的析构函数的类生成移动操作。</font> 

<font color = yellow>
所以仅当下面条件成立时才会生成移动操作（当需要时):

类中没有拷贝操作

类中没有移动操作

类中没有用户定义的析构
</font> 

<br/>





<font color = red>
C++11对于特殊成员函数处理的规则如下：

默认构造函数：仅当类不存在用户声明的构造函数时才自动生成。

析构函数：稍微不同的是现在析构默认noexcept（参见Item14）。和98一样，仅当基类析构为虚函数时该类析构才为虚函数。

拷贝构造函数：逐成员拷贝non-static数据。仅当类没有用户定义的拷贝构造时才生成。
<font color = green>
如果类声明了移动操作它就是delete的。 ======== 当用户声明了拷贝赋值或者析构，该函数自动生成已被废弃。
</font>

拷贝赋值运算符：逐成员拷贝赋值non-static数据。仅当类没有用户定义的拷贝赋值时才生成。
<font color = yellow>
如果类声明了移动操作它就是delete的。========= 当用户声明了拷贝构造或者析构，该函数自动生成已被废弃。
</font>

移动构造函数和移动赋值运算符：都对非static数据执行逐成员移动。<font color = yellow>仅当类没有用户定义的拷贝操作，移动操作或析构时才自动生成。</font>

</font>

<br/>
<br/>

```cpp
注意没有“成员函数模版阻止编译器生成特殊成员函数”的规则。这意味着如果Widget是这样：

class Widget {
    …
    template<typename T>                //从任何东西构造Widget
    Widget(const T& rhs);

    template<typename T>                //从任何东西赋值给Widget
    Widget& operator=(const T& rhs);
    …
};

编译器仍会生成移动和拷贝操作（假设正常生成它们的条件满足），即使可以模板实例化产出拷贝构造和拷贝赋值运算符的函数签名。（当T为Widget时。）很可能你会觉得这是一个不值得承认的边缘情况，但是我提到它是有道理的，Item26将会详细讨论它可能带来的后果。

```


<br/>
<br/>
<br/>
<br/>

<font color = red>
请记住：

特殊成员函数是编译器可能自动生成的函数：默认构造函数，析构函数，拷贝操作，移动操作。

移动操作仅当类没有显式声明移动操作，拷贝操作，析构函数时才自动生成。

拷贝构造函数仅当类没有显式声明拷贝构造函数时才自动生成，并且如果用户声明了移动操作，拷贝构造就是delete。拷贝赋值运算符仅当类没有显式声明拷贝赋值运算符时才自动生成，并且如果用户声明了移动操作，拷贝赋值运算符就是delete。当用户声明了析构函数，拷贝操作的自动生成已被废弃。

成员函数模板不抑制特殊成员函数的生成。
</font>
