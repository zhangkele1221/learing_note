//用 dag 有向无环图设计一个任务调度框架 一个module类 一个executor 类  需要处理 module的依赖关系 使任务可以正常调度运行

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>

using namespace std;

// 定义module 基类
class module {
    public:
        module(string name, vector<string> deps):name_(name),deps_(deps){}
        virtual void execute()=0;//定义一个纯虚函数
        const string& name()const {return name_;}
        const vector<string>& deps() const{return deps_;}
    private:
        string name_;
        vector<string> deps_;
};

//定义moduleA 类 继承自module
class moduleA : public module{
    public:
        moduleA(string name, vector<string> deps):module(name,deps){}
        void execute() override {
            // 这里是moduleA的执行操作
            cout<< "Executing moduleA"<< name() <<endl;
        }
    private:
        string name_;
        vector<string> deps_;
};

//定义moduleB 类 继承自module
class moduleB : public module{
    public:
         moduleB(string name, vector<string> deps):module(name,deps){}
         void execute() override {
            // 这里是moduleA的执行操作
            cout<< "Executing moduleA"<< name() <<endl;
         }
};


//定义 excutor 类 用于处理调度逻辑
class excutor {
    public:
        void add_module(module* mod){
            modules_[mod->name()] = mod;
        }    

        void excutor_all(){
            // 这里实现dag 的调度逻辑
            unordered_map<string,bool> visited;
            for(auto& mod_pair : modules_){
                if(!visited[mod_pair.first]){
                    execute(mod_pair.second,visited);
                }
            }
        }

    private:
        void execute(module* mod,unordered_map<string,bool>& visited){
            visited[mod->name()] = true;
            for(auto& dep: mod->deps()){
                if(!visited[dep]){
                    execute(modules_[dep],visited);
                }
            }
            mod->execute();
        }

        unordered_map<string,module*> modules_;
};

int main(){
    
    //创建moudle 对象 并且指定他们的依赖关系
    moduleA mA1("A1",{});
    moduleA mA2("A2",{"A1"});
    moduleA mB1("B1",{"A1","A2"});
    moduleA mA3("A3",{"B1"});

    //创建excutor 对象  添加 module 对象
    excutor e;
    e.add_module(&mA1);
    e.add_module(&mA2);
    e.add_module(&mB1);
    e.add_module(&mA3);

    //执行所有module 的操作
    e.excutor_all();

    return 0;
}