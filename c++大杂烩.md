
# 模版与范型编程

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