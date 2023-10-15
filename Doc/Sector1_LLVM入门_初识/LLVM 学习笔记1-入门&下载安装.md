## 术语

| 缩语 | 全写                        | 意义       | 备注                                 |
| ---- | --------------------------- | ---------- | ------------------------------------ |
| IR   | intermediate representation | 中间表示   |                                      |
| LLVM | Low Level Virtual Machine   | 底层虚拟机 | LLVM项目本身覆盖的内容已完全超出原意 |
| AST  | Abstract Syntax Tree        | 抽象语法树 |                                      |
| DAG  | Directed Acyclic Graph      | 有向无环图 |                                      |
| JIT  | just-in-time                | 及时编译   |                                      |
| SSA  | Static Single Assignment    | 静态单赋值 |                                      |



## 简介

**官方定义：LLVM是一个模块化和可重用的编译器和工具链技术的集合。**

LLVM最初是2000年由伊利诺伊大学香槟分校(UUIC)的学生Chris Lattner及其硕士顾问Vikram Adve创建的研究项目，并在2003年发布第一个正式版本，目的是提供一种基于SSA的现代编译策略，这种策略能够支持任何编程语言的静态和动态编译。

在编译器之上，各流行语言的编译社区也是两极分化：一种通常提供传统的静态编译器，像 GCC、Free Pascal 和 FreeBASIC；另一种以解释器的形式提供运行时编译或即时编译（JIT）。很少见到一种语言 同时支持这两种编译形式，如果有也是几乎没有复用代码。在过去的十年，LLVM大大的改变了这种状况。LLVM 现在用作通用基础组件，以实现各种静态和运行时编译语言的编译。

LLVM的命名最早源自于``底层虚拟机(Low Level Virtual Machine)``的首字母缩写，由于这个项目的范围并不局限于创建一个虚拟机，这个缩写导致了广泛的疑惑。LLVM开始成长之后，成为众多编译工具及低端工具技术的统称，使得这个名字变得更不贴切，开发者因而决定放弃这个缩写的意涵，现今LLVM已单纯成为一个品牌，适用于LLVM下的所有项目。

**附：clang编译器**
Clang 的开发目标是提供一个可以替代 GCC 的前端编译器。与 GCC 相比，Clang 是一个重新设计的编译器前端，具有一系列优点，例如模块化，代码简单易懂，占用内存小以及容易扩展和重用等。


### LLVM简单介绍
![image](https://user-images.githubusercontent.com/62282139/204067598-0ca1a916-bb87-4915-bf6b-49ee9caf1075.png)


LLVM IR是LLVM的中间表示，是LLVM中很重要的内容，介绍它的文档就单独有一个 https://llvm.org/docs/LangRef.html 

大多数优化都是基于LLVM IR展开。在上图中，opt被画在边上，是为了简化图的内容。因为LLVM的一个设计思想就是优化可以渗透到整个编译器流程的各个阶段，比如编译、链接、运行时等。

在LLVM中，IR有三种表示：

- 可读的IR，类似于汇编代码，介于高等语言和汇编之间，给人看的。后缀.ll

- 不可读的二进制IR,又称bitcode。 后缀为.bc

- 一种内存格式，只保存在内存中，谈不上文件格式，这种格式是LLVM之所以编译快的原因。

三种格式完全等价，我们可以通过clang和llvm工具参数来指定生成这些文件。可以通过llvm-as 和llvm-dis 在前两种文件之间做转换。

在LLVM简化架构图中有个IR linker，这个是IR的链接器，而不是gcc中的那个链接器。LLVM为了实现链接时的优化，在前端(clang)生成整个代码单元的IR后，会将整个工程的IR都链接起来，同时做链接优化。

LLVM backend是LLVM真正的后端，也是LLVM核心。包括编译，汇编，链接等，最后生成汇编文件或者目标码。这里的LLVM comipler和gcc compiler不一样，LLVM compiler仅编译LLVM IR。



### **LLVM编译工具链流程**

![image](https://user-images.githubusercontent.com/62282139/204067606-6049c988-c7bc-4388-a4da-d9132d858855.png)


### LLVM功能介绍

以 `C/C++` 为例，LLVM编译系统包括以下内容：

- 一个良好的前端； `GCC 4.2解析器` 能解析的语言，比如 `C，C ++，Objective-C，Fortran` 等，它都能提供同能能力的支持；另外它还能支持一些GCC的扩展插件。
- LLVM指令集的稳定实现；不管代码处于何种状态，都可以在汇编（ASCII）和字节码（二进制）之间自由转换。
- 一个功能强大的 `Pass` 管理系统，它根据它们的依赖性自动对 `Pass` （包括分析，转换和代码生成Pass）进行排序，并将它们管道化以提高效率。
- 广泛的全局标量优化。
- 包含丰富的分析和转换的链接时过程优化框架，包括复杂的完整程序指针分析、调用图构建以及对配置文件引导优化的支持。
- 易于重定向的代码生成器，目前支持X86，X86-64，PowerPC，PowerPC-64，ARM，Thumb，SPARC，Alpha，CellSPU，MIPS，MSP430，SystemZ和XCore。
- Just-In-Time（JIT）即时编译器，目前支持X86，X86-64，ARM，AArch64，Mips，SystemZ，PowerPC和PowerPC-64。
- 支持生成DWARF调试信息。
- 用于测试和生成除上面列出的目标之外的目标的本机代码的C后端。
- 与gprof类似的分析系统。
- 具有许多基准代码和应用程序的测试框架。
- API和调试工具，以简化LLVM组件的快速开发。



### LLVM的强项

- LLVM使用具有严格定义语义的简单低级语言。
- 它包括C和C++/Objective-C、Java，Scheme等众多的前端（其中也有一部分处于开发之中
- 它包括一个优化器，支持标量、过程间、配置文件驱动和一些简单循环的优化。
- 它支持完整编译模型，包括链接时，安装时，运行时和离线优化。
- LLVM完全支持准确的垃圾回收。
- LLVM代码生成器由于拥强大的目标描述语言所以可以支持众多架构。
- LLVM拥有丰富的文档，各种项目的介绍都非常丰富。
- 许多第三方用户声称LLVM易于使用和开发。例如，Stacker前端（现已不再维护）是在4天内由一个对LLVM小白编写的。此外，LLVM拥有很多帮助新手迅速上手的工具。
- LLVM正在积极开发中，并且不断得到扩展，增强和改进。
- LLVM 的条款非常开放，几乎是随意使用，只要别忘了带上他们的lisence就行。
- LLVM目前由多个商业公司使用，他们开发并贡献了许多扩展和新功能。



## LLVM安装与编译

#### **下载**

下载参见github上链接 ：https://github.com/llvm/llvm-project

安装编译参见tutorials链接： https://llvm.org/docs/GettingStarted.html 

#### LLVM目录结构

- llvm/cmake: 用于产生build文件

- llvm/examples: 一些经典案例，kaleidoscope语言案例和手册等

- llvm/include: 存放 llvm 中作为库的那部分接口代码的 API 头文件。

- llvm/bindings: LLVM基础件的绑定结构，允许使用c\C++之外的语言来使用LLVM，比如Go,Python。

- llvm/project: 一些工程，这里也允许自己构建新工程。

- llvm/test:功能和回归测试，sanity测试等。LLVM 支持一整套完整的测试，测试工具叫 lit，这个路径下放着各种测试用例。

- llvm/utils: 一些实用工具。

- llvm/tools ：一些可执行文件，用于用户的界面使用。

  - lli: LLVM 解释器。可用于直接执行LLVM bitcode.默认情况下，lli作为及时编译器(just-in-time compiler)发挥作用。

  - llvm-dis ：反汇编程序将LLVM bitcode转译为人可读懂的LLVM汇编码。

  - llvm-as: 将.ll文件翻译为.bc文件

  - llc: LLVM后端编译器，将LLVM bitecode转换为本地汇编代码.

  - opt: IR级别做程序优化的工具，输入和输出都是LLVM IR。设计编译器的同学需经常性调用这个工具来验证自己的优化pass是否正确。
  - llvm/lib: llvm编译后生成的目录，存放了大多数的源码。
    - lib/Analysis : 两个 LLVM IR 核心功能之一，各种程序分析，比如变量活跃性分析等。
    - lib/Transforms :  两个 LLVM IR 核心功能之二，做 IR 到 IR 的程序变换，比如死代码消除，常量传播等。
    - lib/IR :  LLVM IR 实现的核心，比如 LLVM IR 中的一些概念，比如 BasicBlock，会在这里定义。
    - lib/AsmParser :  LLVM 汇编的 parser 实现，注意 LLVM 汇编不是机器汇编。
    - lib/Bitcode :  LLVM 位码 \(bitcode\) 的操作。
    - lib/Target :  目标架构下的所有描述，包括指令集、寄存器、机器调度等等和机器相关的信息。这个路径下又会细分不同的后端平台，比如 X86，ARM。
    - lib/CodeGen ：代码生成库的实现核心。LLVM 官方会把后端分为目标相关的（target dependent）代码和目标无关的（target independent）代码。这里就存放这目标无关的代码，比如指令选择，指令调度，寄存器分配等。这里的代码一般情况下不用动，除非你的后端非常奇葩。
    - lib/MC ： 存放与 Machine Code 有关的代码，MC 是后端到挺后边的时候，代码发射时的一种中间表示，也是整个 LLVM 常规编译流程中最后一个中间表示。这里提供的一些类是作为我们 lib/Target/Cpu0 下的类的基类。
 

#### cmake编译命令

在cmake命令中，使用-D<varable name>=<value>的方式来进行LLVM本地配置。常用的配置变量如下所示

| Variable                | Purpose                                                      |
| ----------------------- | ------------------------------------------------------------ |
| CMAKE_C_COMPILER        | Tells `cmake` which C compiler to use. By default, this will be /usr/bin/cc. |
| CMAKE_CXX_COMPILER      | Tells `cmake` which C++ compiler to use. By default, this will be /usr/bin/c++. |
| CMAKE_BUILD_TYPE        | Tells `cmake` what type of build you are trying to generate files for. Valid options are Debug, Release, RelWithDebInfo, and MinSizeRel. Default is Debug. |
| CMAKE_INSTALL_PREFIX    | Specifies the install directory to target when running the install action of the build files. |
| PYTHON_EXECUTABLE       | Forces CMake to use a specific Python version by passing a path to a Python interpreter. By default the Python version of the interpreter in your PATH is used. |
| LLVM_TARGETS_TO_BUILD   | A semicolon delimited list controlling which targets will be built and linked into llvm. The default list is defined as `LLVM_ALL_TARGETS`, and can be set to include out-of-tree targets.The default value includes: `AArch64, AMDGPU, ARM, AVR, BPF, Hexagon, Lanai, Mips, MSP430, NVPTX, PowerPC, RISCV, Sparc, SystemZ, WebAssembly, X86, XCore`. |
| LLVM_ENABLE_DOXYGEN     | Build doxygen-based documentation from the source code This is disabled by default because it is slow and generates a lot of output. |
| LLVM_ENABLE_PROJECTS    | A semicolon-delimited list selecting which of the other LLVM subprojects to additionally build. (Only effective when using a side-by-side project layout e.g. via git). The default list is empty. Can include: clang, clang-tools-extra, cross-project-tests, flang, libc, libclc, lld, lldb, mlir, openmp, polly, or pstl. |
| LLVM_ENABLE_RUNTIMES    | A semicolon-delimited list selecting which of the runtimes to build. (Only effective when using the full monorepo layout). The default list is empty. Can include: compiler-rt, libc, libcxx, libcxxabi, libunwind, or openmp. |
| LLVM_ENABLE_SPHINX      | Build sphinx-based documentation from the source code. This is disabled by default because it is slow and generates a lot of output. Sphinx version 1.5 or later recommended. |
| LLVM_BUILD_LLVM_DYLIB   | Generate libLLVM.so. This library contains a default set of LLVM components that can be overridden with `LLVM_DYLIB_COMPONENTS`. The default contains most of LLVM and is defined in `tools/llvm-shlib/CMakelists.txt`. This option is not available on Windows. |
| LLVM_OPTIMIZED_TABLEGEN | Builds a release tablegen that gets used during the LLVM build. This can dramatically speed up debug builds. |



#### cmake 与LLVM相关的配置参数

cmake配置参数中有不少与LLVM相关的参数配置，具体请参见链接：[Building LLVM with CMake — LLVM 16.0.0git documentation](https://llvm.org/docs/CMake.html#executing-the-tests)

以下列出一些可能常用的配置参数。注意Boolen型参数，其配置值为ON/OFF。

| arg                      | type   | comment                                                      |
| ------------------------ | ------ | ------------------------------------------------------------ |
| LLVM_BUILD_TOOLS         | BOOL   | Build LLVM tools. Defaults to ON. Targets for building each tool are generated in any case. You can build a tool separately by invoking its target. For example, you can build llvm-as with a Makefile-based system by executing make llvm-as at the root of your build directory |
| LLVM_BUILD_TESTS         | BOOL   | Include LLVM unit tests in the ‘all’ build target. Defaults to OFF. Targets for building each unit test are generated in any case. You can build a specific unit test using the targets defined under *unittests*, such as ADTTests, IRTests, SupportTests, etc. (Search for `add_llvm_unittest` in the subdirectories of *unittests* for a complete list of unit tests.) It is possible to build all unit tests with the target *UnitTests*. |
| LLVM_ENABLE_LIBCXX       | BOOL   | If the host compiler and linker supports the stdlib flag, -stdlib=libc++ is passed to invocations of both so that the project is built using libc++ instead of stdlibc++. Defaults to OFF. |
| LLVM_ABI_BREAKING_CHECKS | STRING | Used to decide if LLVM should be built with ABI breaking checks or not. Allowed values are WITH_ASSERTS (default), FORCE_ON and FORCE_OFF. WITH_ASSERTS turns on ABI breaking checks in an assertion enabled build. FORCE_ON (FORCE_OFF) turns them on (off) irrespective of whether normal (NDEBUG-based) assertions are enabled or not. A version of LLVM built with ABI breaking checks is not ABI compatible with a version built without it. |




#### LLVM 编译流程

具体编译流程如下所示

```bash
mkdir llvm_build
cd llvm_build
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX:PATH=../llvm-install -DLLVM_TARGETS_TO_BUILD="X86;RISCV" -DLLVM_BUILD_TOOLS=ON -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lldb" -DLLVM_BUILD_TESTS=ON -DLLVM_ABI_BREAKING_CHECKS=WITH_ASSERTS
make
```

#### 执行LLVM用例测试&回归

[LLVM测试用户手册](https://llvm.org/docs/TestingGuide.html)

LLVM测试支持包括单元测试\回归测试以及整个程序的测试。UT(unit tests)和RT(regression tests)在LLVM仓的``lllvm/unittests``和``llvm/test``。整程序的测试依赖LLVM测试套(test-suit)。

- 单元测试：用例使用Google Test编写并存放在``llvm/unittest``目录。
- 回归测试：用例是小段代码用于测试LLVM特性或触发LLVM中的特定bug。编写语言取决于LLVM的测试特性。这些测试用例使用[lit测试工具](https://llvm.org/docs/CommandGuide/lit.html)。

注意事项：

1. cmake编译参数中需要配置LLVM_BUILD_TEST=ON
2. 若遇到llvm相关命令不存在\不匹配情况，可以过apt方式更新安装，或者直接获取``llvm_build/bin``目录下的对应工具。

```bash
#回归所有LLVM用例，耗时长
make check-all

#进行LLVM单元测试
make check-llvm-unit

#测试个人用例或者部分用例，使用llvm-lit.比如要运行 Integer/BitPacked.ll测试
llvm-lit ~/llvm/test/Integer/BitPacked.ll
```
