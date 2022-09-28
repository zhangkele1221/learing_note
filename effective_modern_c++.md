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

```





