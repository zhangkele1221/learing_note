#include "./EchoApp.h"

DEFINE_SW_APP("EchoServer", "0", sw::SwApp, sw::example::EchoApp);// 这个就相当于 new 一个 merger 或 searcher 这样 其实就是 new 一个 服务节点吧 