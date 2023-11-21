# CRTP 的基本特征表现为：基类是一个模板类；派生类在继承该基类时，将派生类自身作为模板参数传递给基类

## 实现例
```c++
template<typename T>
class baseDemo
{
public:
    virtual ~baseDemo(){}

    void interface(){
        static_cast<T*>(this)->imp();
    }
};

class derivedDemo:public baseDemo<derivedDemo>
{
public:
    void imp(){
        cout << " hello world " << endl;
    }
};
```
## 静态多态
#### 多态是指同一个方法在基类和不同派生类之间有不同的实现，C++ 通过虚函数实现多态，但是虚函数会影响类的内存布局，并且虚函数的调用会增加运行时的开销。

#### CRTP 可以实现静态多态，但本质上 CRTP 中的多个派生类并不是同一个基类，因此严格意义上不能叫多态。
```c++
template<typename T>
class baseDemo
{
public:
    virtual ~baseDemo(){}
    void interface() { static_cast<T*>(this)->imp(); }
    void imp() { cout << "imp hello world " << endl; }
};

class derivedDemo1:public baseDemo<derivedDemo1>
{
public:
    void imp(){ cout << "derivedDemo1 hello world " << endl; }
};

class derivedDemo2 :public baseDemo<derivedDemo2>
{
public:
    void imp() { cout << "derivedDemo2 hello world " << endl; }
};

template<typename T>
void funcDemo(T & base)
{
    base.interface();
}

int main()
{
    derivedDemo1 d1;
    derivedDemo2 d2;

    funcDemo(d1);
    funcDemo(d2);
   
    return 0;
}
derivedDemo1 hello world
derivedDemo2 hello world

```

### 使用 CRTP 可以把重复性的代码抽象到基类中，减少代码冗余。

```c++
template<typename T>
class baseDemo
{
public:
    virtual ~baseDemo(){}
    void getType() { 
        T& t = static_cast<T&>(*this);
        cout << typeid(t).name() << endl;
    }
  
};

class derivedDemo1:public baseDemo<derivedDemo1>
{
};

class derivedDemo2 :public baseDemo<derivedDemo2>
{
};

int main()
{
    derivedDemo1 d1;
    derivedDemo2 d2;

    d1.getType();//class derivedDemo1
    d2.getType();//class derivedDemo2
    
    return 0;
}

```
### 扩展既有类的功能 使用 CRTP 可以在基类中调用派生类的成员函数，从而可以在调用前后扩展新的操作。
```c++
template<typename T>
class baseDemo
{
public:
    virtual ~baseDemo() {}
    void interface(){
        cout << "hello " << endl;
        static_cast<T*>(this)->imp();
    }
};

class derivedDemo :public baseDemo<derivedDemo>
{
public:
    void imp(){
        cout << "derivedDemo " << endl;
    }
};

int main()
{
    derivedDemo demo;
    demo.interface();
    
    return 0;
}
```


#### 应用 CRTP 可以把一个类变为单例模式，
```c++
template<typename T>
class singlePatternTemplate
{
public:
    virtual ~singlePatternTemplate() {}
    singlePatternTemplate(const singlePatternTemplate&) = delete;
    singlePatternTemplate & operator=(const singlePatternTemplate&) = delete;

    static T& getSingleObj(){
        static T obj;
        return obj;
    }
protected:
    singlePatternTemplate(){}
};

class derivedDemo :public singlePatternTemplate<derivedDemo>
{
    friend singlePatternTemplate<derivedDemo>;
private:
    derivedDemo(){}
};

```
