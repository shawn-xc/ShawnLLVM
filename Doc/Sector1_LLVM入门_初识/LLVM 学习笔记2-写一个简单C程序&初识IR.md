## 一个简单的C程序

### 操作详细步骤

- 在example目录新建一个目录，写一个hello.c代码。

```c
#include <stdio.h>
int main() {
  printf("hello world\n");
  return 0;
}
```

- hello.c文件目录下执行 clang编译器，生成本地执行文件。

  ```bash
  clang hello.c -o hello
  ```

- 这是传统的编译器使用方法,可使用通用方式运行程序

  ```bash
  ./hello
  ```

- 使用clang 将C文件编译为LLVM bitcode文件。注意，此处的-emit-llvm选项和-c选项一起，告知llvm用于发出.bc文件。-o3表示优化等级。

  ```
  clang -O3 -emit-llvm hello.c -c -o hello.bc 
  ```

- 使用llvm专属运行程序方式执行程序,lli可理解为llvm解释器(interpreter)

  ```bash
  lli hello.bc
  ```

- 使用llvm-dis命令来查看LLVM bitcode汇编代码

  ```bash
  llvm-dis < hello.bc | less
  ```

- 也可以通过llvm-dis将位码文件转为可读的IR文件(.ll)

  ```bash
  llvm-dis hell.bc
  ```

- 使用llc命令进行后端编译，生成本地汇编代码

  ```bash
  llc hello.bc -o hello.s
  ```

- 使用通用编译器将汇编代码组装为运行程序

  ```bash
  gcc hello.s -o hello.native 
  ```

- 运行本地程序

  ```bash
  ./hello.native
  ```

### 实战总结

#### 总结1-gcc的编译流程

gcc编译一般分为四个阶段，分别是预处理、编译、汇编、链接。

1. 预处理的作用是进行宏展开和头文件替换
2. 编译的作用是将C代码转成汇编代码   
3. 汇编的作用是将汇编代码转成对应的二进制CPU指令   ()
4. 链接的作用是将多个代码之间关联起来，形成一个完整的程序。

#### 总结2-LLVM文件后缀名

LLVM部分文件后缀解释说明

- .ll后缀名文件：   可读LLVM IR文件

- .bc后缀名文件： 不可读LLVM IR文件

- .s后缀名文件：    汇编代码文件，LLVM IR的可读汇编显示。

通过llvm-dis命令可以对.bc文件进行汇编形式呈现，或者直接通过clang -emit-llvm 将源代码生成.ll文件，

#### 总结3-LLVM IR初识

使用``llvm-dis``命令进行IR文件呈现时，IR内容以规范的格式呈现，如下所示

```assembly
; ModuleID = 'hello.bc'
source_filename = "hello.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [13 x i8] c"Hello World\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  %2 = call i32 (ptr, ...) @printf(ptr noundef @.str)
  ret i32 0
}

declare i32 @printf(ptr noundef, ...) #1

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 15.0.2"}
```

由上述IR内容可知LLVM IR的结构
![image](https://user-images.githubusercontent.com/62282139/204068029-5b81b7c3-8b28-4ab1-9848-866f1a28f8d4.png)


- module(模块)是一份LLVM IR的顶层容器，对应于编译前端的每个翻译单元(translationUnit)。每个模块由目标机器信息、全局符号(全局变量\函数)以及元信息组成。
- Function(函数) ：就是编程语言中的函数，包括函数签名和若干个基本块，函数的第一个基本块叫做入口基本块。
- BasicBlock(基本块) 是一组顺序执行的指令集合，只有一个入口和一个出口，非头尾指令执行时不会违背顺序跳转到其他指令上去。每个基本块的最后一条指令一般是跳转指令(跳转到其他基本块),函数内的最后一个基本块的最后条指令就是函数返回指令。
- Instruction(指令) 是LLVM IR中的最小可执行单位，每条指令都单独占一行。

##### LLVM IR 头信息

LLVM IR头是一些target information

```assembly
; ModuleID = 'hello.bc'
source_filename = "hello.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"
```

- ModuleID：编译器用于区分不同模块的ID
- source_filename：源文件名
- target triple：用于描述目标机器信息的一个元组，一般形式是<architecture>-<vendor>-<system>[-extra-info]
- target datalayout：目标机器架构数据布局。它是由-分隔的一组规格数据，部分内容解析如下
  - e：内存存储模式为小端模式
  - m:o：目标文件的格式是Mach格式
  - i64:64：64位整数的对齐方式是64位，即8字节对齐
  - f80:128：80位扩展精度浮点数的对齐方式是128位，即16字节对齐
  - n8:16:32:64：整型数据有8位的、16位的、32位的和64位的
  - S128：128位栈自然对齐
  - 更多datalayout的内容对齐请见文档: https://llvm.org/docs/LangRef.html#data-layout

#### 更多LLVM IR的内容学习

IR作为LLVM框架的关键桥梁，接下来我们还将对IR的数据表示、类型系统、控制语句、函数等内容进行学习。详情请参见 "LLVM IR学习"系列文章。


