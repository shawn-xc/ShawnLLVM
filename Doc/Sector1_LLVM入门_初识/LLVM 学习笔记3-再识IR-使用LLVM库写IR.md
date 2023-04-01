## 使用LLVM库写IR文件

我们再次实战写个相对复杂的代码，通过利用LLVM API编写一个简单的生成LLVM IR的独立程序。从而，初步了解LLVM IR的内存表示形式。
在此章，需要重点关注IR文件的具体意义和构建方法，以及编译LLVM库可能遇到的问题。

####  简单C++程序

1. 编写一个简单的c++程序

```c++
void foo(int x) {
    for (int i = 0; i < 10; i++) {
        if (x % 2 == 0) {
            x += i;
        } else {
            x -= i;
        }
    }
}
```

2. 通过clang编译出LLVM IR位码文件

```bash
clang++ -emit-llvm test.cpp -c -o test.bc
llvm-dis test.bc
```

3. 生成的IR文件(汇编) test.ll如下所示(部分)

```assembly
; ModuleID = 'test.cpp'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define dso_local void @_Z3fooi(i32 %x) #0 {
entry:
  %x.addr = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 %x, i32* %x.addr, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, i32* %i, align 4
  %cmp = icmp slt i32 %0, 10
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %1 = load i32, i32* %x.addr, align 4
  %rem = srem i32 %1, 2
  %cmp1 = icmp eq i32 %rem, 0
  br i1 %cmp1, label %if.then, label %if.else

if.then:                                          ; preds = %for.body
  %2 = load i32, i32* %i, align 4
  %3 = load i32, i32* %x.addr, align 4
  %add = add nsw i32 %3, %2
  store i32 %add, i32* %x.addr, align 4
  br label %if.end

if.else:                                          ; preds = %for.body
  %4 = load i32, i32* %i, align 4
  %5 = load i32, i32* %x.addr, align 4
  %sub = sub nsw i32 %5, %4
  store i32 %sub, i32* %x.addr, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  br label %for.inc

attributes #0 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"clang version 13.0.0 (ssh://git@codehub-dg-y.huawei.com:2222/y00469409/llvm_riscv.git b5481d071ff3860766bb93cfa384bb59390723c2)"}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.mustprogress"}

```

### 编写生成LLVM IR的程序

通过使用LLVM API来编写上面显示的LLVM IR汇编文件，包括内容:创建模块、创建函数、设置运行时抢占符、设置函数属性、设置函数参数名称、创建基本块、分配栈内存、内存写入、无条件跳转、比较大小、有条件跳转、二元运算、函数返回等。

#### # 创建模块

模块(module)是LLVM IR的最顶层实体，LLVM IR程序是由模块组成，每个模块对应输入程序的一个编译单元。如下代码显示创建一个``模块ID``为 ``test.cpp``的模块对象``M``。该对象的生命周期通过智能指针``std::unique_ptr``进行管理

```c++
LLVMContext Ctx;
std::unique_ptr<Module> M(new Module("test.cpp", Ctx));
```

#### 创建函数

由于 C++ 程序会进行名称重编。因此，函数foo的真正名称为重编后的名称(这里是_Z3fooi)。创建一个函数原型为``void _Z3fooi(int)``的全局函数，由类``llvm::FunctionType``的对象表示。

```c++
// 构造向量FuncTyAgrs，其初始元素数量应该等于要创建函数的参数个数。避免不必要的性能开销
SmallVector<Type *, 1> FuncTyAgrs;
FuncTyAgrs.push_back(Type::getInt32Ty(Ctx));

// FunctionType::get()的第三个参数表示是否带可变参数。if false，表示函数没有可变参数...
auto *FuncTy = FunctionType::get(Type::getVoidTy(Ctx), FuncTyAgrs, false);

// Function::Createt()的第二个参数用于指定函数的链接属性。
// 如果值为Function::ExternalLinkage，表示该函数是全局函数。
auto *FuncFoo =
    Function::Create(FuncTy, Function::ExternalLinkage, "_Z3fooi", M.get());
```

#### 设置运行时抢占符

将函数funcFoo所表示函数的运行时抢占符设置为``dso_local``

```c++
FuncFoo->setDSOLocal(true);
```

#### 设置函数属性

为``FuncFoo``所表示的函数属性,属性类型定义在``llvm_build/include/llvm/IR/Attributes.inc``目录中。

```c++
// 设置函数属性（数值形式)
FuncFoo->addFnAttr(Attribute::NoInline);
FuncFoo->addFnAttr(Attribute::NoUnwind);

// 设置函数属性（字符串形式）
AttrBuilder FuncAttrs;
FuncAttrs.addAttribute("disable-tail-calls", llvm::toStringRef(false));
FuncAttrs.addAttribute("frame-pointer", "all");
FuncFoo->addAttributes(AttributeList::FunctionIndex, FuncAttrs);
```

#### 设置函数参数名称

```c++
// 将FuncFoo所表示函数的（从左到右）第一个参数的名称设置为x
Function::arg_iterator Args = FuncFoo->arg_begin();
Args->setName("x");
```

#### 创建基本块

在``FuncFoo``所表示的函数中创建一个标签为``entry``的基本块，由类``llvm::BasicBlock``的对象表示。

```c++
// BasicBlock::Create()的第四个参数用于指定基本块的前继节点。
// 如果值为nullptr，表示要创建的基本块无前继节点。
BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", FuncFoo, nullptr);
```

#### 分配栈内存

在基本块``EntryBB``的末尾添加一条``alloca``指令，并使用4字节对齐。
**注：**AllocaInst构造函数中第二个参数用于指定地址空间。若为0，表示默认地址空间。

```c++
auto *AIX = new AllocaInst(Type::getInt32Ty(Ctx), 0, "x.addr", EntryBB);
AIX->setAlignment(Align(4));
```

#### 内存写入

在基本块``EntryBB``的末尾添加一条``store``指令，并且以4字节对齐。``store i32 %x, i32* %x.addr, align 4``

```c++
// 第三个参数表示是否指定volatile属性。如果值为false，表示不指定
auto *StX = new StoreInst(X, AIX, false, EntryBB);
StX->setAlignment(Align(4));
```

#### 无条件跳转

在基本块``EntryBB``的末尾添加一条``br``指令(无条件跳转)，从基本块``EntryBB``跳转到``ForCondBB``。``br label %for.cond``

```c++
BranchInst::Create(ForCondBB, EntryBB);
```

#### 比较大小

在基本块``ForCondBB``的末尾添加一条``icmp``指令，比较结果保存到局部变量``Cmp``中。``%cmp = icmp slt i32 %0, 10``（判断局部标识符%0的值是否小于10。若是，%cmp值为true）

```c++
auto *Cmp = new ICmpInst(*ForCondBB, ICmpInst::ICMP_SLT,
    Ld0, ConstantInt::get(Ctx, APInt(32, 10)));
Cmp->setName("cmp");
```

#### 有条件跳转

在基本块``ForCondBB``的末尾添加一条``br``指令（有条件跳转）。即如果局部变量``Cmp``所表示的值为true，则跳转到基本块``ForBodyBB``；否则，跳转到基本块``ForEndBB``。``br i1 %cmp, label %for.body, label %for.end``

```c++
BranchInst::Create(ForBodyBB, ForEndBB, Cmp, ForCondBB);
```

#### 二元运算

在基本块``IfThenBB``的末尾添加一条``add``指令，并且加法运算的属性设置为``nsw``。``%add = add nsw i32 %3, %2``表示将局部标识符%3和%2的值相加，结果保存在%add中。

```c++
auto *Add = BinaryOperator::Create(Instruction::Add, Ld3, Ld2, "add", IfThenBB);
Add->setHasNoSignedWrap();
```

#### 函数返回

在基本块``ForEndBB``的末尾添加一条``ret``指令（无返回值）。``ret void``

```c++
// 第二个参数表示函数的返回值类型。如果值为nullptr，表示返回值的类型为void
ReturnInst::Create(Ctx, nullptr, ForEndBB);
```

#### 检查IR是否合法

```c++
if (verifyModule(*M)) {
  errs() << "Error: module failed verification. This shouldn't happen.\n";
  exit(1);
}
```

#### 生成位码文件

```c++
std::error_code EC;
std::unique_ptr<ToolOutputFile> Out(
    new ToolOutputFile("./test.bc", EC, sys::fs::F_None));
if (EC) {
  errs() << EC.message() << '\n';
  exit(2);
}
WriteBitcodeToFile(*M, Out->os());

// 用于指示不要删除生成的位码文件
Out->keep();
```

#### 完成代码程序

```c++
#include "llvm/ADT/SmallVector.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <string>
#include <system_error>

using namespace llvm;

static MDNode *getID(LLVMContext &Ctx, Metadata *arg0 = nullptr,
                     Metadata *arg1 = nullptr) {
  MDNode *ID;
  SmallVector<Metadata *, 3> Args;
  // Reserve operand 0 for loop id self reference.
  Args.push_back(nullptr);

  if (arg0)
    Args.push_back(arg0);
  if (arg1)
    Args.push_back(arg1);

  ID = MDNode::getDistinct(Ctx, Args);
  ID->replaceOperandWith(0, ID);
  return ID;
}

// define dso_local void @_Z3fooi(i32 %x) #0 {...}
static Function *createFuncFoo(Module *M) {
  assert(M);

  SmallVector<Type *, 1> FuncTyAgrs;
  FuncTyAgrs.push_back(Type::getInt32Ty(M->getContext()));
  auto *FuncTy =
      FunctionType::get(Type::getVoidTy(M->getContext()), FuncTyAgrs, false);

  auto *FuncFoo =
      Function::Create(FuncTy, Function::ExternalLinkage, "_Z3fooi", M);

  Function::arg_iterator Args = FuncFoo->arg_begin();
  Args->setName("x");

  return FuncFoo;
}

static void setFuncAttrs(Function *FuncFoo) {
  assert(FuncFoo);

  static constexpr Attribute::AttrKind FuncAttrs[] = {
      Attribute::NoInline, Attribute::NoUnwind,     Attribute::OptimizeNone,
      Attribute::UWTable,  Attribute::MustProgress,
  };

  for (auto Attr : FuncAttrs) {
    FuncFoo->addFnAttr(Attr);
  }

  static std::map<std::string, std::string> FuncAttrsStr = {
      {"disable-tail-calls", "false"},
      {"frame-pointer", "all"},
      {"less-precise-fpmad", "false"},
      {"min-legal-vector-width", "0"},
      {"no-infs-fp-math", "false"},
      {"no-jump-tables", "false"},
      {"no-nans-fp-math", "false"},
      {"no-signed-zeros-fp-math", "false"},
      {"no-trapping-math", "true"},
      {"stack-protector-buffer-size", "8"},
      {"target-cpu", "x86-64"},
      {"target-features", "+cx8,+fxsr,+mmx,+sse,+sse2,+x87"},
      {"tune-cpu", "generic"},
      {"unsafe-fp-math", "false"},
      {"use-soft-float", "false"},
  };

  AttrBuilder Builder;
  for (auto Attr : FuncAttrsStr) {
    Builder.addAttribute(Attr.first, Attr.second);
  }
  FuncFoo->addAttributes(AttributeList::FunctionIndex, Builder);
}

int main() {
  LLVMContext Ctx;
  std::unique_ptr<Module> M(new Module("test.cpp", Ctx));

  M->setSourceFileName("test.cpp");
  M->setDataLayout(
      "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128");
  M->setTargetTriple("x86_64-unknown-linux-gnu");

  auto *FuncFoo = createFuncFoo(M.get());
  FuncFoo->setDSOLocal(true);
  setFuncAttrs(FuncFoo);

  BasicBlock *EntryBB = BasicBlock::Create(Ctx, "entry", FuncFoo, nullptr);
  BasicBlock *ForCondBB = BasicBlock::Create(Ctx, "for.cond", FuncFoo, nullptr);

  // entry:
  //   %x.addr = alloca i32, align 4
  //   %i = alloca i32, align 4
  //   store i32 %x, i32* %x.addr, align 4
  //   store i32 0, i32* %i, align 4
  //   br label %for.cond
  auto *AIX = new AllocaInst(Type::getInt32Ty(Ctx), 0, "x.addr", EntryBB);
  AIX->setAlignment(Align(4));
  auto *AII = new AllocaInst(Type::getInt32Ty(Ctx), 0, "i", EntryBB);
  AII->setAlignment(Align(4));
  auto *StX = new StoreInst(FuncFoo->arg_begin(), AIX, false, EntryBB);
  StX->setAlignment(Align(4));
  auto *StI =
      new StoreInst(ConstantInt::get(Ctx, APInt(32, 0)), AII, false, EntryBB);
  StI->setAlignment(Align(4));
  BranchInst::Create(ForCondBB, EntryBB);

  BasicBlock *ForBodyBB = BasicBlock::Create(Ctx, "for.body", FuncFoo, nullptr);
  BasicBlock *ForEndBB = BasicBlock::Create(Ctx, "for.end", FuncFoo, nullptr);

  // for.cond:                                         ; preds = %for.inc,
  // %entry
  //   %0 = load i32, i32* %i, align 4
  //   %cmp = icmp slt i32 %0, 10
  //   br i1 %cmp, label %for.body, label %for.end
  auto *Ld0 = new LoadInst(Type::getInt32Ty(Ctx), AII, "", false, ForCondBB);
  Ld0->setAlignment(Align(4));
  auto *Cmp = new ICmpInst(*ForCondBB, ICmpInst::ICMP_SLT, Ld0,
                           ConstantInt::get(Ctx, APInt(32, 10)));
  Cmp->setName("cmp");
  BranchInst::Create(ForBodyBB, ForEndBB, Cmp, ForCondBB);

  BasicBlock *IfThenBB = BasicBlock::Create(Ctx, "if.then", FuncFoo, nullptr);
  BasicBlock *IfElseBB = BasicBlock::Create(Ctx, "if.else", FuncFoo, nullptr);

  // for.body:                                         ; preds = %for.cond
  //   %1 = load i32, i32* %x.addr, align 4
  //   %rem = srem i32 %1, 2
  //   %cmp1 = icmp eq i32 %rem, 0
  //   br i1 %cmp1, label %if.then, label %if.else
  auto *Ld1 = new LoadInst(Type::getInt32Ty(Ctx), AIX, "", false, ForBodyBB);
  Ld1->setAlignment(Align(4));
  auto *Rem = BinaryOperator::Create(Instruction::SRem, Ld1,
                                     ConstantInt::get(Ctx, APInt(32, 2)), "rem",
                                     ForBodyBB);
  auto *Cmp1 = new ICmpInst(*ForBodyBB, ICmpInst::ICMP_EQ, Rem,
                            ConstantInt::get(Ctx, APInt(32, 0)));
  Cmp1->setName("cmp1");
  BranchInst::Create(IfThenBB, IfElseBB, Cmp1, ForBodyBB);

  BasicBlock *IfEndBB = BasicBlock::Create(Ctx, "if.end", FuncFoo, nullptr);

  // if.then:                                          ; preds = %for.body
  //   %2 = load i32, i32* %i, align 4
  //   %3 = load i32, i32* %x.addr, align 4
  //   %add = add nsw i32 %3, %2
  //   store i32 %add, i32* %x.addr, align 4
  //   br label %if.end
  auto *Ld2 = new LoadInst(Type::getInt32Ty(Ctx), AII, "", false, IfThenBB);
  Ld2->setAlignment(Align(4));
  auto *Ld3 = new LoadInst(Type::getInt32Ty(Ctx), AIX, "", false, IfThenBB);
  Ld3->setAlignment(Align(4));
  auto *Add =
      BinaryOperator::Create(Instruction::Add, Ld3, Ld2, "add", IfThenBB);
  Add->setHasNoSignedWrap();
  auto *StAdd = new StoreInst(Add, AIX, false, IfThenBB);
  StAdd->setAlignment(Align(4));
  BranchInst::Create(IfEndBB, IfThenBB);

  // if.else:                                          ; preds = %for.body
  //   %4 = load i32, i32* %i, align 4
  //   %5 = load i32, i32* %x.addr, align 4
  //   %sub = sub nsw i32 %5, %4
  //   store i32 %sub, i32* %x.addr, align 4
  //   br label %if.end
  auto *Ld4 = new LoadInst(Type::getInt32Ty(Ctx), AII, "", false, IfElseBB);
  Ld4->setAlignment(Align(4));
  auto *Ld5 = new LoadInst(Type::getInt32Ty(Ctx), AIX, "", false, IfElseBB);
  Ld5->setAlignment(Align(4));
  auto *Sub =
      BinaryOperator::Create(Instruction::Sub, Ld5, Ld4, "sub", IfElseBB);
  Sub->setHasNoSignedWrap();
  auto *StSub = new StoreInst(Sub, AIX, false, IfElseBB);
  StSub->setAlignment(Align(4));
  BranchInst::Create(IfEndBB, IfElseBB);

  BasicBlock *ForIncBB = BasicBlock::Create(Ctx, "for.inc", FuncFoo, nullptr);

  // if.end:                                           ; preds = %if.else,
  // %if.then
  //   br label %for.inc
  BranchInst::Create(ForIncBB, IfEndBB);

  // for.inc:                                          ; preds = %if.end
  //   %6 = load i32, i32* %i, align 4
  //   %inc = add nsw i32 %6, 1
  //   store i32 %inc, i32* %i, align 4
  //   br label %for.cond, !llvm.loop !2
  auto *Ld6 = new LoadInst(Type::getInt32Ty(Ctx), AII, "", false, ForIncBB);
  Ld6->setAlignment(Align(4));
  auto *Inc = BinaryOperator::Create(Instruction::Add, Ld6,
                                     ConstantInt::get(Ctx, APInt(32, 1)), "inc",
                                     ForIncBB);
  Inc->setHasNoSignedWrap();
  auto *StInc = new StoreInst(Inc, AII, false, ForIncBB);
  StInc->setAlignment(Align(4));
  auto *BI = BranchInst::Create(ForCondBB, ForIncBB);
  {
    SmallVector<Metadata *, 1> Args;
    Args.push_back(MDString::get(Ctx, "llvm.loop.mustprogress"));
    auto *MData =
        MDNode::concatenate(nullptr, getID(Ctx, MDNode::get(Ctx, Args)));
    BI->setMetadata("llvm.loop", MData);
  }

  // for.end:                                          ; preds = %for.cond
  //   ret void
  ReturnInst::Create(Ctx, nullptr, ForEndBB);

  // !llvm.module.flags = !{!0}
  // !0 = !{i32 1, !"wchar_size", i32 4}
  {
    llvm::NamedMDNode *IdentMetadata = M->getOrInsertModuleFlagsMetadata();
    Type *Int32Ty = Type::getInt32Ty(Ctx);
    Metadata *Ops[3] = {
        ConstantAsMetadata::get(ConstantInt::get(Int32Ty, Module::Error)),
        MDString::get(Ctx, "wchar_size"),
        ConstantAsMetadata::get(ConstantInt::get(Ctx, APInt(32, 4))),
    };
    IdentMetadata->addOperand(MDNode::get(Ctx, Ops));
  }

  // !llvm.ident = !{!1}
  // !1 = !{!"clang version 12.0.0 (ubuntu1)"}
  {
    llvm::NamedMDNode *IdentMetadata =
        M->getOrInsertNamedMetadata("llvm.ident");
    std::string ClangVersion("clang version 12.0.0 (...)");
    llvm::Metadata *IdentNode[] = {llvm::MDString::get(Ctx, ClangVersion)};
    IdentMetadata->addOperand(llvm::MDNode::get(Ctx, IdentNode));
  }

  if (verifyModule(*M)) {
    errs() << "Error: module failed verification. This shouldn't happen.\n";
    exit(1);
  }

  std::error_code EC;
  std::unique_ptr<ToolOutputFile> Out(
      new ToolOutputFile("./test.bc", EC, sys::fs::OF_None));
  if (EC) {
    errs() << EC.message() << '\n';
    exit(2);
  }
  WriteBitcodeToFile(*M, Out->os());
  Out->keep();

  return 0;
}

```


#### 代码编译命令

```bash
clang++ $(llvm-config_l --cxxflags) main_ir.cpp -o main_ir $(llvm-config_l --ldflags --libs) -lpthread -lncurses -lz
```

#### 程序执行

通过本地执行main_ir即可得到与之前类似的位码文件。

```bash
./main_ir
```

###编译&环境问题总结

#### llvm-config使用操作

在进行include llvm库的代码文件编译时，需要将依赖的头文件和库进行编译，llvm通过提供``llvm-config``工具支持快速集成llvm相关头文件和库链接。
``llvm-config --cxxflags``提供关键头文件和宏定义集成

```bash
$ llvm-config --cxxflags
-I/usr/local/include -std=c++14   -fno-exceptions -fno-rtti -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
```

``llvm-config --ldflags --system-libs``提供关键库链接

```bash
$ llvm-config --ldflags --system-libs --libs core
-L/usr/local/lib 
-lLLVMCore -lLLVMRemarks -lLLVMBitstreamReader -lLLVMBinaryFormat -lLLVMSupport -lLLVMDemangle
-lrt -ldl -lpthread -lm -lz -lzstd.so.1.4.9 -ltinfo -lxml2
```

组合起来使用的常用命令行如下

```bash
clang++ -g  `llvm-config_l --cxxflags` toy.cpp -o toy `llvm-config_l --ldflags --system-libs --libs core`
```

#### 使用命令行编译可能遇到的问题

1. 在通过命令行对llvm关联代码进行编译译过程中，可能遇到一些问题，比如：

- 调用``apt``提供的``llvm-config``命令，链接报错zstd.so.1.4.9不存在
- ``apt``安装的llvm不支持``llvm-li``命令
- ``apt``提供的命令行与``llvm_build/bin``提供的命令版本不兼容
  建议直接通过alias将llvm相关的命令重新定向到llvm_build/bin目录下

2. llvm_build/bin提供的llvm-config --libs内容过少问题
   一般是缺少``-lpthread`` ``-lncurses`` ``-lz``,在编译命令中追加即可.



