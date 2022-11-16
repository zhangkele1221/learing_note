## 一次c++异常问题的定位
```
(gdb) bt
#0 0x0000003cf592e2ed in raise () from /lib64/tls/libc.so.6
#1 0x0000003cf592fa3e in abort () from /lib64/tls/libc.so.6
#2 0x0000003cf86b1138 in __gnu_cxx::__verbose_terminate_handler () from /usr/lib64/libstdc++.so.6
#3 0x0000003cf86af166 in __cxa_call_unexpected () from /usr/lib64/libstdc++.so.6
#4 0x0000003cf86af193 in std::terminate () from /usr/lib64/libstdc++.so.6
#5 0x0000003cf86af293 in __cxa_throw () from /usr/lib64/libstdc++.so.6
#6 0x0000000000400e9f in test ()
#7 0x0000000000400f9d in main ()

从上述调用栈，可以得知异常由__cxa_throw ()抛出，可在该函数处设置断点，从而得知“异常出自哪里”。

上述给出的获取异常调用函数信息的方法，应该和操作系统和调试器无关，其他平台类似。


1) 直接获取异常的相关调用函数，在相应函数处设置断点。
利用前一步骤的信息，直接b __cxa_throw，即可设置有效断点。
继续 c 
发送请求 

此时 core 的堆栈信息就会 清晰了


2) 利用gdb的catch throw/catch
该方法也很通用，但有一个需要注意的地方：在程序执行之前，catch throw/catch是无效的，需要在程序执行之后(先在main处设置断点)，使用catch throw才有效。


```

# core dump 问题解决方法汇总

## gdb 和 addr2line 调试 crash（包含如何调试so里面的crash） dmesg + addr2line
来自 https://blog.csdn.net/lishenglong666/article/details/80913018

先看下面
```c++
#include <stdio.h>

int main()
{
        int *p = NULL;
        *p = 0;

        printf("bad\n");
        return 0;
}

```
g++  -g  -Wall  -o  gdb  gdb.cc 
./gdb
Segmentation fault (core dumped)

执行：  dmesg  | grep gdb

[480339.445434] traps: gdb[1187728] trap divide error ip:56498312c17b sp:7ffc74ebefe0 error:0 in gdb[56498312c000+1000]
[481624.863208] gdb[1191782]: segfault at 0 ip 00005619fd8c0161 sp 00007ffceec2d140 error 6 in gdb[5619fd8c0000+1000]
[482010.601877] gdb[1193320]: segfault at 0 ip 000056534ee71161 sp 00007ffd7b43fc80 error 6 in gdb[56534ee71000+1000]

这里其实已经可以看出来问题了 ip 000056534ee71161 这个地址就可以追踪到问题了

如果不确定
执行

objdump -d  gdb

输出 这样的函数指令地址

```
0000000000001149 <main>:
    1149:       f3 0f 1e fa             endbr64 
    114d:       55                      push   %rbp
    114e:       48 89 e5                mov    %rsp,%rbp
    1151:       48 83 ec 10             sub    $0x10,%rsp
    1155:       48 c7 45 f8 00 00 00    movq   $0x0,-0x8(%rbp)
    115c:       00 
    115d:       48 8b 45 f8             mov    -0x8(%rbp),%rax
    1161:       c7 00 00 00 00 00       movl   $0x0,(%rax)
    1167:       48 8d 3d 96 0e 00 00    lea    0xe96(%rip),%rdi        # 2004 <_IO_stdin_used+0x4>
    116e:       e8 dd fe ff ff          callq  1050 <puts@plt>
    1173:       b8 00 00 00 00          mov    $0x0,%eax
    1178:       c9                      leaveq 
    1179:       c3                      retq   
    117a:       66 0f 1f 44 00 00       nopw   0x0(%rax,%rax,1)

0000000000001180 <__libc_csu_init>:
    1180:       f3 0f 1e fa             endbr64 
    1184:       41 57                   push   %r15
    1186:       4c 8d 3d 2b 2c 00 00    lea    0x2c2b(%rip),%r15        # 3db8 <__frame_dummy_init_array_entry>
    118d:       41 56                   push   %r14
    118f:       49 89 d6                mov    %rdx,%r14
    1192:       41 55                   push   %r13
    1194:       49 89 f5                mov    %rsi,%r13
    1197:       41 54                   push   %r12
    1199:       41 89 fc                mov    %edi,%r12d
    119c:       55                      push   %rbp
    119d:       48 8d 2d 1c 2c 00 00    lea    0x2c1c(%rip),%rbp        # 3dc0 <__do_global_dtors_aux_fini_array_entry>
    11a4:       53                      push   %rbx
    11a5:       4c 29 fd                sub    %r15,%rbp
    11a8:       48 83 ec 08             sub    $0x8,%rsp
    11ac:       e8 4f fe ff ff          callq  1000 <_init>
    11b1:       48 c1 fd 03             sar    $0x3,%rbp
    11b5:       74 1f                   je     11d6 <__libc_csu_init+0x56>
    11b7:       31 db                   xor    %ebx,%ebx
    11b9:       0f 1f 80 00 00 00 00    nopl   0x0(%rax)
    11c0:       4c 89 f2                mov    %r14,%rdx
    11c3:       4c 89 ee                mov    %r13,%rsi
    11c6:       44 89 e7                mov    %r12d,%edi
    11c9:       41 ff 14 df             callq  *(%r15,%rbx,8)
    11cd:       48 83 c3 01             add    $0x1,%rbx
    11d1:       48 39 dd                cmp    %rbx,%rbp
    11d4:       75 ea                   jne    11c0 <__libc_csu_init+0x40>
    11d6:       48 83 c4 08             add    $0x8,%rsp
    11da:       5b                      pop    %rbx
    11db:       5d                      pop    %rbp
    11dc:       41 5c                   pop    %r12
    11de:       41 5d                   pop    %r13
    11e0:       41 5e                   pop    %r14
    11e2:       41 5f                   pop    %r15
    11e4:       c3                      retq   
    11e5:       66 66 2e 0f 1f 84 00    data16 nopw %cs:0x0(%rax,%rax,1)
    11ec:       00 00 00 00 

可以看到  1161  这个地址的函数汇编 

使用命令 
addr2line -e  gdb  1161
执行后可以清晰的看出哪一行代码出现了问题
/home/test_use_tool/gdb.cc:6
```


## 方法四: strace + addr2line
https://blog.csdn.net/stpeace/article/details/49852065

