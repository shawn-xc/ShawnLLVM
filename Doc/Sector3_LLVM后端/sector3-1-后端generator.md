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

![image](https://github.com/shawn-xc/ShawnLLVM/assets/62282139/770a4b5a-53d4-4c32-bf2c-0de57f178710)


如上所示，从``LLVM IR``开始一直到汇编代码或者目标代码，涵盖了整个后端整个过程。请注意，白框全部是passes，灰色框是具体的操作环节，（其实也是由一系列的passes组成，但他们是生成代码所必要的）。而白框的pass是为了提升效率，可以理解为非必要。

基于流程图，我们对后端流程进行简要说明：

1. **指令选择**(``Instruction Selection``): 指令选择阶段将内存中的``LLVM IR``表示转换为制定目标的``SelectionDAG``节点。该阶段一开始将``LLVM IR``的三地址结构转换为有向无环图(DAG)形式。此步骤将基于目标指令集确定一种高效方式来表征LLVM输入代码。每个DAG图对应一个基本块内的指令，即每个基本块都与一个不同的DAG图相关联，DAG图中的节点通常表示指令。基于DAG图的转换是后端中的重要环节，以便允许LLVM代码生成器利用基于模式匹配的指令选择算法，此算法经过修改也可以作用于DAG。到此阶段结束为止，DAG图中所有的``LLVM IR``节点都会转换为目标机器节点，即每个节点代表目标机器指令，而不是LLVM指令。
2. **指令调度**(``Instruction Scheduling``): 指令调度阶段使用``SelectionDAG``,确定指令顺序，然后将指令作为目标机器指令发出。在完成指令选择后，编译器已经知道应该使用哪些目标指令来执行每个基本块的计算，这些信息都包含在``SelectionDAG``中。单DAG图并不包含没有依赖关系的指令间的顺序，所以还需要返回三地址指令形式，已确定基本块内的指令顺序。此处的指令调度也称为**前寄存器分配调度**(``Pre-Register Allocation Scheduling``),它负责尽可能多地优化指令级并行度的同时对指令进行排序。所有指令接下来都被转换为机器指令``MachineInstr``三地址形式表示形式。
3. **寄存器分配**(``Register Allocation``): 目标机器代码从SSA格式的虚拟寄存器文件转换为具体的寄存器文件。寄存器分配阶段将从程序中消除所有虚拟寄存器的引用。在之前的学习中我们有了解到，``LLVM IR``拥有的是一组无限(虚拟)寄存器，这个特性将一直持续到寄存器分配之前。在寄存器分配阶段，无限虚拟寄存器引用将被转换为特定目标的有限寄存器集，在有些场景下会产生溢出(``spill``)。
4. **指令调度**(``Instruction Scheduling``): 在寄存器分配之后进行的指令调度又称为**后寄存器指令调度**(``post-register allocation scheduling``)。由于此时目标机器的寄存器信息已经可用，编译器根据硬件资源的竞争关系和不同寄存器的访问延时差异性进一步提升生成的代码质量。
5. **``Prolog``/``Epilog``代码插入**：`Prolog`和`Epilog`对应的中文意义为函数的"序言"和"尾声"，是在函数开始和结束时要插入的代码。一旦为函数生成了机器码，并且已知所需的堆栈空间，就可以插入函数的``prolog``和``epilog``代码。此阶段负责实现帧指针消除和堆栈打包等优化。
6. **代码发射**(``Code Emission``)：以汇编形式或者以目标机器代码形式输出最终的代码。此阶段负责将``MachineInstr``表示的指令转换为``MCInst``实例。``MCInst``的表示形式更适合与汇编器和链接器，通常有两种选择：输出汇编代码或者将二进制大对象输出为特定的目标代码格式。

通过上述的简单说明，我们可以观察到LLVM在整个后端使用了四种不同层次的指令表达形式：**内存中的``LLVM IR``**，**``SelectionDAG``节点**，**``MachineInstr``以及``MCInst``**。
