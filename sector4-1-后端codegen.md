##  1 简介

本文为LLVM[``与后端无关的代码生成器``](https://releases.llvm.org/10.0.0/docs/CodeGenerator.html)学习笔记，希望帮助从零开始的同事对编译器后端和LLVM后端有初步的了解。

LLVM ``与目标无关的代码生成器 CodeGenerator``是一个框架，它提供了一套可重用的组件用于将LLVM IR转化为指定目标环境的机器码。这个``CodeGenerator``包括六个主要组件：

1. 目标的抽象描述层(`Target`)。此层捕获目标机器的各个方面的重要属性，并使他们的使用与目标无关。这些接口在``/llvm/Target``中定义。
2. 代码生成的源头类(``CodeGen``)。这些类旨在能够足够抽象的表示任何目标计算机的机器码。这些类在``/llvm/CodeGen/``中定义，在此层上，一些基本概念比如``constant pool enrtires``和``jump tables``是显式存在的。
3. ``MC(Machine Code)``层、目标对象文件层次级别的表示代码的类和算法。这些类用于表征汇编级的代码构建，比如标签、块和指令。
4. 目标无关的算法(寄存器分配、调度、堆栈帧表示等)，这些代码在``lib/CodeGen/``中。
5. 目标机器描述信息的应用层。目标机器描述利用LLVM提供的组件可以自定义地选择pass优化，进而基于特定目标机器进行代码生成。这些内容在``Lib/Target/``中。
6. 与目标无关的``JIT``组件。``LLVM JIT``完全独立于目标环境之外，这些内容在``lib/ExecutionEngine/JIT``中。

上述组件对应的一些参考链接如下：

-  目标描述：[target description](https://releases.llvm.org/10.0.0/docs/CodeGenerator.html#target-description) 
-  机器码表示层：[machine code representation](https://releases.llvm.org/10.0.0/docs/CodeGenerator.html#machine-code-representation) 
-  如何使用目标描述：[implement the target description](https://releases.llvm.org/10.0.0/docs/CodeGenerator.html#implement-the-target-description) 
-  LLVM代码表示：[LLVM code representation](https://releases.llvm.org/10.0.0/docs/LangRef.html)
-  代码生成算法：[code generation algorithm](https://releases.llvm.org/10.0.0/docs/CodeGenerator.html#code-generation-algorithm)

##  2 LLVM后端

###  2.1 后端流程

LLVM后端流程如下流程图所示：

![image-1.1](picture/1.1.PNG)

如上所示，从``LLVM IR``开始一直到汇编代码或者目标代码，涵盖了整个后端整个过程。请注意，白框全部是passes，灰色框是具体的操作环节，（其实也是由一系列的passes组成，但他们是生成代码所必要的）。而白框的pass是为了提升效率，可以理解为非必要。

基于流程图，我们对后端流程进行简要说明：

1. **指令选择**(``Instruction Selection``): 指令选择阶段将内存中的``LLVM IR``表示转换为制定目标的``SelectionDAG``节点。该阶段一开始将``LLVM IR``的三地址结构转换为有向无环图(DAG)形式。此步骤将基于目标指令集确定一种高效方式来表征LLVM输入代码。每个DAG图对应一个基本块内的指令，即每个基本块都与一个不同的DAG图相关联，DAG图中的节点通常表示指令。基于DAG图的转换是后端中的重要环节，以便允许LLVM代码生成器利用基于模式匹配的指令选择算法，此算法经过修改也可以作用于DAG。到此阶段结束为止，DAG图中所有的``LLVM IR``节点都会转换为目标机器节点，即每个节点代表目标机器指令，而不是LLVM指令。
2. **指令调度**(``Instruction Scheduling``): 指令调度阶段使用``SelectionDAG``,确定指令顺序，然后将指令作为目标机器指令发出。在完成指令选择后，编译器已经知道应该使用哪些目标指令来执行每个基本块的计算，这些信息都包含在``SelectionDAG``中。单DAG图并不包含没有依赖关系的指令间的顺序，所以还需要返回三地址指令形式，已确定基本块内的指令顺序。此处的指令调度也称为**前寄存器分配调度**(``Pre-Register Allocation Scheduling``),它负责尽可能多地优化指令级并行度的同时对指令进行排序。所有指令接下来都被转换为机器指令``MachineInstr``三地址形式表示形式。
3. **寄存器分配**(``Register Allocation``): 目标机器代码从SSA格式的虚拟寄存器文件转换为具体的寄存器文件。寄存器分配阶段将从程序中消除所有虚拟寄存器的引用。在之前的学习中我们有了解到，``LLVM IR``拥有的是一组无限(虚拟)寄存器，这个特性将一直持续到寄存器分配之前。在寄存器分配阶段，无限虚拟寄存器引用将被转换为特定目标的有限寄存器集，在有些场景下会产生溢出(``spill``)。
4. **指令调度**(``Instruction Scheduling``): 在寄存器分配之后进行的指令调度又称为**后寄存器指令调度**(``post-register allocation scheduling``)。由于此时目标机器的寄存器信息已经可用，编译器根据硬件资源的竞争关系和不同寄存器的访问延时差异性进一步提升生成的代码质量。
5. **``Prolog``/``Epilog``代码插入**：`Prolog`和`Epilog`对应的中文意义为函数的"序言"和"尾声"，是在函数开始和结束时要插入的代码。一旦为函数生成了机器码，并且已知所需的堆栈空间，就可以插入函数的``prolog``和``epilog``代码。此阶段负责实现帧指针消除和堆栈打包等优化。
6. **代码发射**(``Code Emission``)：以汇编形式或者以目标机器代码形式输出最终的代码。此阶段负责将``MachineInstr``表示的指令转换为``MCInst``实例。``MCInst``的表示形式更适合与汇编器和链接器，通常有两种选择：输出汇编代码或者将二进制大对象输出为特定的目标代码格式。

通过上述的简单说明，我们可以观察到LLVM在整个后端使用了四种不同层次的指令表达形式：**内存中的``LLVM IR``**，**``SelectionDAG``节点**，**``MachineInstr``以及``MCInst``**。

###  2.2 后端流程细化

在不考虑PASS节点情况下，我们将LLVM后端流程进一步细化，得如下所示流程图：

![image-1.2](./picture/1.2.PNG)

1. ``SelectionDAGBuilder``遍历``LLVM IR``中的每一个``function``以及``function``中的每一个``basic block``，并将其中的指令转为``SDNode``(``SelectionDAGNode``)结构。整个``function``或``basic block``转成``SelectionDAG``。此时每个``node``中依然是``LLVM IR``指令。
2. ``SelectionDAG``经过``legalization``(合法化)和其它``optimizations``，DAG节点被映射到目标指令,这个映射过程是``Instruciton Selection``指令选择。这时的DAG中的``LLVM IR``节点转换成了目标架构节点，也就是将``LLVM IR`` 指令转换成了机器指令. 所以这时候的DAG又称为``machineDAG``。
3. 在``machineDAG``已经是机器指令，可以用来执行``basic block``中的运算。所以可以在``machineDAG``上做``instruction scheduling``，以确定``basic block``中指令的执行顺序。寄存器分配前的指令调度器实际有2个: 作用于``SelectionDAG``，发射线性序列指令。主要考虑指令级的平行性。经过这个scheduling后的指令转换成了`MachineInstr`三地址表示。指令调度器有三种类型：``list scheduling algo``, ``fast algo``, ``vliew``.
4. 寄存器分配是将虚拟寄存器``virtual Register``分配到物理寄存器``physical Register``，并优化寄存器分配过程使溢出最小化。虚拟寄存器到物理寄存器的的映射有2种方式：直接映射和间接映射。直接映射利用``TargetRegisterInfo``和``MachineOperand``类获取``load``/``store``指令插入位置，以及从内容去除和存入的值。间接映射利用``VirtRegMap``类处理``load``/``store``指令。寄存器分配算法有4种：``Basic Register Allocator``、``Fast Register Allocator``、``PBQP Register Allocato``、``Greedy Register Allocator``。
5. `Post-RA-Scheduling`也就是寄存器分配后的指令调度阶段，其作用于机器指令``MachineInstr``。这时能得到物理寄存器信息，可以结合物理寄存器的安全性和执行效率，对指令顺序做调整。
6. 完成指令调度的机器指令将用于代码发射`CodeEmission`，将`MachineInstr`转为`MCInst`,`EmitToStreamer`函数将`MCInst`发射为二进制文件或汇编代码。

###  2.3 后端代码结构

LLVM后端主要使用lib目录及子文件夹内容，比如子文件夹:``Codegen``,``MC``,``TableGen``和``Target``。这些文件夹名称与LLVM相关概念上是对应上的。

- ``codegen``目录包含所有通用代码生成算法的实现文件和头文件：指令选择、指令调度、寄存器分配以及他们所需要的辅助分析函数。
- ``MC``目录包含汇编器(汇编语言分析程序)、松弛算法(反汇编器)和具体的对象文件(比如ELF、COFF、Macho等)等底层功能的实现。
- ``TableGen``目录包含``TableGen``工具的完整实现，该工具用于根据``.td``文件中的高阶目标描述来生成C++代码。
- ``Target``目录下包含每个目标环境的具体实现(比如``Target/Mpis``)，通常包含多个``.cpp``,`` .h``和``.td``文件。在不同目标中实现类似功能的文件通常共享相似的名称。

##  3 目标

目标描述性信息被存放在``.td``文件中(``.td``文件为``TableGen``的处理文件)。目标描述信息存放在目标描述类集``Target Description classes``中，这些目标信息通常具有大量的公共信息，为了允许最大限度的考虑公共性，LLVM代码生成器使用了``TableGen``来进行目标计算机的描绘。

###  3.1 目标描述

LLVM 目标描述类簇( ``Target Description Classes``)提供了一个抽象的针对任意后端机器的通用描述，这些类被用来管理一些抽象的目标属性(比如指令和寄存器)，并且不夹杂任何代码生成的算法的片段。除了``DataLayout``外，其他所有的目标描述类都需要作为基类被特定目标的描述类所继承，同时提供虚函数实现。目标描述簇包括：

1.  ``TargetMachine``类。``TargetMachine``类提供了一些虚函数和特定目标相关的访问器，被命名为 ``get*Info ``方法，比如 ``getInstrInfo``、``getRegisterInfo``、``getFrameInfo`` 等。这个类需要被特定目标的类所继承，比如 X86 中继承 ``TargetMachine`` 实现的 ``X86TargetMachine``，其中需要实现各种虚函数。唯一必须依赖的类是 ``DataLayout``，但是如果其他功能类也被使用的话，它们同样也需要被实现。
2. ``DataLayout`` 类。``DataLayout`` 类是唯一必须依赖的目标描述类，而且不能被继承。 ``DataLayout ``指定了一些与目标 ``layout`` 有关的信息，比如内存结构，各种数据类型的对齐要求，指针占用空间，大小端的要求。
3. ``TargetLowering`` 类。``TargetLowering`` 类被 ``SelectionDAG`` 组件所使用，在指令选择器中发挥功能，它承担将高阶表达形式``LLVM IR`` 降低到低阶表达形式``SelectionDAG``的工作。
4. ``TargetRegisterInfo`` 类。``TargetRegisterInfo`` 类用于描述与目标相关的寄存器信息以及任何与寄存器的交互动作。
5. ``TargetInstrInfo`` 类。``TargetInstrInfo`` 类被用于描述特定目标支持的机器指令码。会描述指定对应的助记符和操作码、操作数的数量、隐式寄存器 ``uses`` 和 ``defs``、一些目标无关的属性（比如指令是否访问内存、是否可交换）、以及维护目标平台的 flag。
6. ``TargetFrameLowering`` 类。``TargetFrameLowering`` 类被用于提供一些目标栈帧相关的信息。比如栈的生长方向，进入函数的栈对齐要求和对局部数据的偏移。对局部数据的偏移是针对进入函数的栈指针的偏移，这里还存储一些处理函数的数据，如局部变量、 spill 位置等。
7. ``TargetSubtarget`` 类。``TargetSubtarget`` 类被用于提供一些与目标特定芯片相关的信息，可以简单理解为目标机器的子目标。
8. ``TargetJITInfo`` 类。``TargetJITInfo`` 类提供了一个抽象的接口，用于支持特定目标的``Just-In-Time`` 代码生成器的工作。

###  3.2 机器代码描述

LLVM 代码会转化为成由 ``MachineFunction``、``MachineBasicBlock`` 和 ``MachineInstr`` 实例组成的特定机器的表示（机器代码描述类``Machine Code Description Classes``）。这种表示完全是目标无关的，以最抽象的方式描述指令：操作码和操作数。

#### 1 ``MachineInstr`` 类

``MachineInstr``是机器指令的意思，缩写为`MI`。

操作码是一个简单的无符号整型数，只在特定后端下才有效。所有的指令都是在`` *InstrInfo.td ``文件中定义的，操作码的枚举值仅仅是依据这份描述自动生成。``MachineInstr ``类没有任何有关于指令意义的信息（比如这条指令的意义是什么），必须依赖于`` TargetInstrInfo ``类来了解。

操作数可以有多种不同类型，比如寄存器引用、常量值、basic block 引用等。另外，操作数应该被标记为 def 或 use （仅寄存器可以标记为 def ）。

根据管理，LLVM代码生成器对指令操作数进行排序，以便所有寄存器定义都在寄存器使用之前。比如SPARC的add指令：“add %i1, %i2, %i3”,它将``%i1``寄存器和``%i2``寄存器相加并将结果存储到``%i3``寄存器。在LLVM代码生成器中，操作数根据目的优先原则应存储为“``%i3``, ``%i2``, ``%i1``”。将目标操作数保留在操作数列表的开头有几个优点，特别是调试打印，比如：

```assembly
%r3 = add %i1, %i2
```
