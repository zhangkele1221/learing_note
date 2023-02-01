
# linux 内核的两个关键宏 
## offsetof 原理
```c++
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

说明：获得结构体(TYPE)的变量成员(MEMBER)在此结构体中的偏移量。

(01)  ( (TYPE *)0 )   将零转型为TYPE类型指针，即TYPE类型的指针的地址是0。
(02)  ((TYPE *)0)->MEMBER     访问结构中的数据成员。
(03)  &( ( (TYPE *)0 )->MEMBER )     取出数据成员的地址。由于TYPE的地址是0，这里获取到的地址就是相对MEMBER在TYPE中的偏移。
(04)  (size_t)(&(((TYPE*)0)->MEMBER))     结果转换类型。对于32位系统而言，size_t是unsigned int类型；对于64位系统而言，size_t是unsigned long类型。


#include <stdio.h>
#include <iostream>

// 获得结构体(TYPE)的变量成员(MEMBER)在此结构体中的偏移量。 
// stdlib 库中也有这个宏的实现
#define OFFSET(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER) 

class student
{
public:
    char gender;
    double id;
    int age;
};

int main()
{
    int gender_offset, id_offset, age_offset;

    gender_offset = OFFSET(student, gender);
    id_offset = OFFSET(student, id);
    age_offset = OFFSET(student, age);

    std::cout<<gender_offset<<std::endl;
    std::cout<<id_offset<<std::endl;
    std::cout<<age_offset<<std::endl;
    
    return 1;
}

0
8
16

```
## container_of 原理
```c++
#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

说明：根据"结构体(type)变量"中的"域成员变量(member)的指针(ptr)"来获取指向整个结构体变量的指针。
(01) typeof( ( (type *)0)->member )     取出member成员的变量类型。
(02) const typeof( ((type *)0)->member ) *__mptr = (ptr)    定义变量__mptr指针，并将ptr赋值给__mptr。经过这一步，__mptr为member数据类型的常量指针，其指向ptr所指向的地址。
(04) (char *)__mptr    将__mptr转换为字节型指针。
(05) offsetof(type,member))    就是获取"member成员"在"结构体type"中的位置偏移。
(06) (char *)__mptr - offsetof(type,member))    就是用来获取"结构体type"的指针的起始地址（为char *型指针）。
(07) (type *)( (char *)__mptr - offsetof(type,member) )    就是将"char *类型的结构体type的指针"转换为"type *类型的结构体type的指针"。

=======================
#include <stdio.h>
#include <string.h>

// 获得结构体(TYPE)的变量成员(MEMBER)在此结构体中的偏移量。
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

// 根据"结构体(type)变量"中的"域成员变量(member)的指针(ptr)"来获取指向整个结构体变量的指针
#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct student
{
    char gender;
    int id;
    int age;
    char name[20];
};

void main()
{
    struct student stu;
    struct student *pstu;

    stu.gender = '1';
    stu.id = 9527;
    stu.age = 24;
    strcpy(stu.name, "zhouxingxing");

    // 根据"id地址" 获取 "结构体的地址"。
    pstu = container_of(&stu.id, struct student, id);

    // 根据获取到的结构体student的地址，访问其它成员
    printf("gender= %c\n", pstu->gender);
    printf("age= %d\n", pstu->age);
    printf("name= %s\n", pstu->name);
}

gender= 1
age= 24
name= zhouxingxing

```

# 模版与范型编程

## 重载与模版
像普通函数一样，模板也是可以重载的。也就是说，你可以定义多个有相同函数名的函数， 当实际调用的时候，由 C++编译器负责决定具体该调用哪一个函数

```c++
#include <iostream>
#include <string>
#include <memory>
#include <sstream>

using namespace std;

 // maximum of two int values:
int max (int a, int b)
{
    cout<<" int "<<endl;
    return b < a ? a : b;
}
  // maximum of two values of any type:
template<typename T> T max (T a, T b)
{
    cout<<" template "<<endl;
    return b < a ? a : b;
}

int main()
{
    ::max(7, 42); // int 不用解释了
    ::max(7.0, 42.0); // 如果模板可以实例化出一个更匹配的函数，那么就会选择这个模板
    ::max('a', 'b'); //如果模板可以实例化出一个更匹配的函数，那么就会选择这个模板
    ::max<>(7, 42); // 也可以显式指定一个空的模板列表。这表明它会被解析成一个模板调用 其所有的模板参 数会被通过调用参数推断出来
    ::max<double>(7, 42); // 同上 无需推断了
    ::max('a', 42.7); // 模板参数推断时不允许自动类型转换，而常规函数是允许的，因此这个调用会 选择非模板参函数
    ::max(100, 42.7); //同上
    ::max(100.1, 42); // 同上
}

 int 
 template 
 template 
 template 
 template 
 int 
 int 
 int 

```


## 可变参数模版
  一个可变参数模版 是一个接受可变数目参数的模版函数或模版类  注意两个包 一个叫模版参数包 一个叫函数参数包
```c++
#include <iostream>
#include <string>
using namespace std;

template <typename T, typename... Args>
void foo(const T&t, const Args&... rest){
    cout<<sizeof...(Args)<<" ";// Args 模版参数包 表示0个或多个模版类型参数
    cout<<sizeof...(rest)<<endl;// rest 函数参数包 表示0个或多个函数参数
}

int main()
{
    int i =0; double d =3.14; string s ="Hello World";
    foo(i,s,42,d);
    foo(s,42,"hi");
    foo("hi");
    cout<<"Hello World";
    return 0;
}

3 3
2 2
0 0
Hello World

```

### 编写可变参数函数模版

```c++
#include <iostream>
#include <string>
using namespace std;

// 此函数 必须在可变参数版本的print 定义之前声明
template <typename T>
ostream & print(ostream& os,const T &t){
    return os<< t<< endl;    
}

template <typename T ,typename... Args>
ostream & print(ostream& os,const T &t, const Args&... rest){
    os<< t<< " ,";
    return print(os,rest...);
}

int main()
{
    int i =0 ;
    string s = "Hello";
    print(cout,i);
    print(cout,i,s);
    return 0;
}

0
0 ,Hello

```

### 包扩展
补充....
### 转发参数包
补充....