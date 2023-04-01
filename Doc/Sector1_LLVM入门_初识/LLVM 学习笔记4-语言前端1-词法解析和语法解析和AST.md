### 前言

本文章开始到后续几篇学习文章都是 LLVM手册 ``我的第一个语言前端``内容的实践，主要包括原手册英文汉化以及实践过程中的个人经验总结。

手册详见链接：https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html

### 简介

在``我的第一个语言前端``实践中，将假设用户理解并掌握``c++``，对于是否有编译器经验并不要求。在接下来的几次学习中，我们学习使用kaleidoscope万花筒语言来进行开发和构建。具体包括8个章节。

- 第一章：kaleidoscope语言及其词法解析器。第一章也是本文的主要内容，用于介绍kaleidoscope(万花筒)语言具备的基本功能，以及词法解析器。为方便理解，本文中的词法解析器没有采用任何词法分析器和语法分析器，直接使用c++功能实现。
- 第二章：语法分析器和AST。 完成词法分析之后,进行语法解析技术学习，进而开始构造基本的AST。在第二章共介绍了**递归下降解析**和**运算符优先级解析**两种解析技术。
- 第三章: LLVM IR代码生成。搞定AST之后，学习进行LLVM IR的生成。
- 第四章 添加JIT和优化支持。很多人都有意将LLVM用作``JIT``，有鉴于此，在第四章学习区区三行代码搞定JIT支持。
- 第五章：语言扩展：流程控制。  我们的语言已经可以运行了，在第五章为它添加流程控制能力（即`if`/`then`/`else`和“`for`”循环）。
- 第六章：语言扩展：用户自定义运算符。第六章再次对语言进行了扩展，赋予了用户在程序中自行定义任意一元和二元运算符的能力。
- 第七章 语言扩展：可变变量。  第七章介绍的是用户自定义局部变量和赋值运算符的实现。
- 第八章：结论及其LLVM相关的内容总结。
### 第一章·kaleidoscope语言

kaleidoscope(万花筒)语言是一种过程式语言，用户可以定义函数、使用条件语句、执行数学运算等。我们将逐步扩展Kaleidoscope，为它增加`if`/`then`/`else`结构、`for`循环、用户自定义运算符，以及带有简单命令行界面的JIT编译器等等。

简单起见，Kaleidoscope只支持一种数据类型，即64位浮点数（也就是C中的“double”）。这样一来，所有的值都是双精度浮点数，类型申明也省了，语言的语法简洁明快。以下是一个Kaleidoscope语言用于计算[[斐波那契数](https://zh.m.wikipedia.org/zh-hans/%E6%96%90%E6%B3%A2%E9%82%A3%E5%A5%91%E6%95%B0)]的简单示例：

```python
# 方式一 Compute the x'th fibonacci number.
def fib(x)
  if x < 3 then
    1
  else
    fib(x-1)+fib(x-2)

# This expression will compute the 40th number.
fib(40)

#方式二 利用函数库实现斐波那契数列
extern sin(arg)
extern cos(arg)
extern atan2(arg1 arg2)

atan2(sin(.4), cos(42))
```

### 第一章·词法分析器(The Lexer)

要实现一门语言，第一要务就是要能够处理文本文件，搞明白其中究竟写了些什么。传统上，我们会先利用“词法分析器”将输入切成“语元（token）”，然后再做处理。词法分析器返回的每个语元都带有一个语元编号(用于标定语元类型)，此外可能还会附带一些元数据（比如数值\字符串）。

比如对于如下代码

```c++
atan2(sin(.4), cos(42))
```

通过词法分析器后应该转为

```c++
tokens = ["atan2", "(", "sin", "(", .4, ")", ",", "cos", "(", 42, ")", ")"]
```

基于``斐波那契数``案例我们尝试写一个简单的词法分析器。

#### 词法分析器·语元定义

```c++
// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  tok_eof = -1,         // 文本结束

  // commands
  tok_def = -2,         // define 语元
  tok_extern = -3,      // extern 语元

  // primary
  tok_identifier = -4,   // 变量名
  tok_number = -5        // 数值
};

static std::string IdentifierStr;  // Filled in if tok_identifier(for 字符串/变量)
static double NumVal;              // Filled in if tok_number (for 数值)
```

上述代码定义了一些5种语元类型，词法分析器返回的语元要么是上述5个语元枚举值之一，要么是诸如“`+`”这样的尚未定义的字符。对于后一种情况，词法分析器返回的是这些字符的ASCII值。如果当前语元是标识符，其名称将被存入全局变量`IdentifierStr`。如果当前语元是数值常量（比如`1.0`），其值将被存入`NumVal`。(注意，简单起见，我们动用了全局变量，在真正的语言实现中这可不是最佳选择) 。

#### 词法分析器·gettok()

Kaleidoscope的词法分析器由一个名为`gettok`的函数实现。调用该函数，就可以得到标准输入中的下一个语元。它的开头是这样的：

```c++
/// gettok - Return the next token from standard input.
static int gettok() {
	static int LastChar = ' ';

	// Skip any whitespace. 
    // 如有空白符一直读，一直读到一个非空白字符。
	while (isspace(LastChar))
    	LastChar = getchar();
```

`gettok`通过C标准库的`getchar()`函数从标准输入中逐个读入字符。它一边识别读取的字符，一边将最后读入的字符存入`LastChar`，留待后续处理。这个函数干的第一件事就是利用上面的循环剔除语元之间的空白符。

接下来，`gettok`开始识别**标识符**和“`def`”、“`extern`”等关键字。这个任务由``isalpha``触发实现：

```c++
  if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]* //首字符必须是字母
      IdentifierStr = LastChar;                                // 将字符存入到string数组
      while (isalnum((LastChar = getchar())))                  // 接下来的字符可以是字母表或者是数字
          IdentifierStr += LastChar;                           // 字符拼接

      if (IdentifierStr == "def") return tok_def;              // 触发define 语元
      if (IdentifierStr == "extern") return tok_extern;        // 触发extern 语元
      return tok_identifier;
  }
```

注意，标识符一被识别出来就被存入全局变量`IdentifierStr`。此外，语言中的关键字也由这个循环负责识别，在此处一并处理。

数值的识别过程与此类似：

```c++
  if (isdigit(LastChar) || LastChar == '.') {   // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');    // 遇到数字和.就一直拼接下来，哪怕是0.1.22.33.44

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }
```

处理输入字符的代码简单明了。只要碰到代表数值的字符串，就用C标准库中的`strtod`函数将之转换为数值并存入`NumVal`。下面我们来处理注释：

```c++
  if (LastChar == '#') {                                   //在任意位置遇到#,直到行位都是注释
    // Comment until end of line.
    do LastChar = getchar();                               // 在遇到EOF,\n,\r之前不停读
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');   

    if (LastChar != EOF)
      return gettok();
  }
```

注释的处理很简单：直接跳过注释所在的那一行，然后返回下一个语元即可。

最后，如果碰到上述情况都处理不了的字符，那么只有两种可能：要么碰到了表示运算符的字符（比如“`+`”），要么就是已经读到了文件末尾。这两种情况由以下代码负责处理：

```c++
  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}
```

至此，完整的Kaleidoscope词法分析器就完成了。

### 第二章·抽象语法树(AST)

抽象语法树的作用在于牢牢抓住程序的脉络，从而方便编译过程的后续环节（如代码生成）对程序进行解读。AST就是开发者为语言量身定制的一套模型，基本上语言中的每种结构都与一种AST对象相对应。Kaleidoscope语言中的语法结构包括**表达式**、**函数原型**和**函数对象**。我们不妨先从表达式入手：

#### AST·表达式

```c++
//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() {}
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;
public:
  NumberExprAST(double val) : Val(val) {}
};
```

上述代码定义了基类`ExprAST`和一个用于表示数值常量的子类。其中子类`NumberExprAST`将数值常量的值存放在成员变量中，以备编译器后续查询。

当前我们还只搭出了AST的架子，尚未定义任何能够体现AST实用价值的成员方法。例如，只需添加一套虚方法，我们就可以轻松实现代码的格式化打印。以下代码定义了Kaleidoscope语言最基本的部分所要用到的其他各种表达式的AST节点：

```c++
/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(const std::string &name) : Name(name) {}
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(const std::string &callee, std::vector<ExprAST*> &args)
    : Callee(callee), Args(args) {}
};
```

上述几个类设计得简单明了：`VariableExprAST`用于保存变量名，`BinaryExprAST`用于保存运算符（如“`+`”），`CallExprAST`用于保存函数名和用作参数的表达式列表。这样设计AST有一个优势，那就是我们无须关注语法便可直接抓住语言本身的特性。注意这里还没有涉及到二元运算符的优先级和词法结构等问题。

定义完毕这几种表达式节点，就足以描述Kaleidoscope语言中的几个最基本的结构了。由于我们还没有实现条件控制流程，它还不算[图灵完备](https://baike.baidu.com/item/%E5%9B%BE%E7%81%B5%E5%AE%8C%E5%A4%87/4634934)；后续还会加以完善。接下来，还有两种结构需要描述，即函数的接口和函数本身：

```c++
/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
public:
  PrototypeAST(const std::string &name, const std::vector<std::string> &args)
    : Name(name), Args(args) {}
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  PrototypeAST *Proto;
  ExprAST *Body;
public:
  FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}
};
// 代码描述了一个具体函数的AST组成，包括函数类型(PrototypeAST用于明确函数名和函数参数) 和 函数实现(ExprAST)
```

在Kaleidoscope中，函数的类型是由参数的个数决定的。由于所有的值都是双精度浮点数，没有必要保存参数的类型。在更强大、更实用的语言中，`ExprAST`类多半还会需要一个类型字段。

有了这些作为基础，我们就可以开始解析Kaleidoscope的表达式和函数体了。

### 第二章·语法分析器基础(Parser Basic)

开始构造AST之前，先要准备好用于构造AST的语法解析器。说白了，就是要利用语法解析器把“`x+y`”这样的输入（由词法分析器返回的三个语元）分解成由下列代码生成的AST：

```c++
ExprAST *X = new VariableExprAST("x");
ExprAST *Y = new VariableExprAST("y");
ExprAST *Result = new BinaryExprAST('+', X, Y);
```

我们先定义几个辅助函数：

```c++
/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() {
  return CurTok = gettok();
}
```

这段代码以词法分析器为中心，实现了一个简易的语元缓冲，让我们能够预先读取词法分析器将要返回的下一个语元。在我们的语法解析器中，所有函数都将`CurTok`视作当前待解析的语元。

```c++
/// Error* - These are little helper functions for error handling.
ExprAST *Error(const char *Str) { fprintf(stderr, "Error: %s\n", Str);return 0;}
PrototypeAST *ErrorP(const char *Str) { Error(Str); return 0; }
FunctionAST *ErrorF(const char *Str) { Error(Str); return 0; }
```

这三个用于报错的辅助函数也很简单，我们的语法解析器将用它们来处理解析过程中发生的错误。这里采用的错误恢复策略并不妥当，对用户也不怎么友好，但对于教程而言也就够用了。示例代码中各个函数的返回值类型各不相同，有了这几个函数，错误处理就简单了：它们的返回值统统是`NULL`。

准备好这几个辅助函数之后，我们就开始实现第一条语法规则：数值常量。

### 第二章·解析表达式(Basic Expression Parsing)

之所以先从数值常量下手，是因为它最简单。**Kaleidoscope语法中的每一条生成规则（production），都需要一个对应的解析函数**。

#### 数值常量 规则解析

```c++
/// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
  ExprAST *Result = new NumberExprAST(NumVal);
  getNextToken(); // consume the number
  return Result;
}
```

这个函数很简单：调用它的时候，当前待解析语元定义只能是`tok_number(枚举)`。该函数用刚解析出的数值构造出了一个`NumberExprAST`节点，然后令词法分析器继续读取下一个语元，最后返回构造的AST节点。

这里有几处很有意思，其中最显著的便是该函数的行为，它不仅消化了所有与当前生成规则相关的所有语元，还把下一个待解析的语元放进了词法分析器的语元缓冲（该语元与当前的生成规则无关）。这是非常标准的**递归下降解析器(recursive descent parsers)**的行为。

#### 括号运算符 规则解析

```c++
/// parenexpr ::= '(' expression ')' 例如(4)
static ExprAST *ParseParenExpr() {
  getNextToken();  // eat (.
  ExprAST *V = ParseExpression();
  if (!V) return 0;

  if (CurTok != ')')
    return Error("expected ')'");
  getNextToken();  // eat ).
  return V;
}
// 括号表达式至少处理了3个语元，ParseExpression的具体实现待定义，我们可以假设其可以从左括号一直处理到右括号(暂不考虑多括号的情况)
```

该函数体现了这个语法解析器的几个特点：

1. 它展示了`Error`函数的用法。调用该函数时，待解析的语元只能是“`(`”，然而解析完子表达式之后，紧跟着的语元却不一定是“`)`”。比如，要是用户输入的是“`(4 x`”而不是“`(4)`”，语法解析器就应该报错。既然错误时有发生，语法解析器就必须提供一条报告错误的途径：就这个语法解析器而言，应对之道就是返回`NULL`。
2. 该函数的另一特点在于递归调用了`ParseExpression`（很快我们就会看到`ParseExpression`还会反过来调用`ParseParenExpr`）。这种手法简化了递归语法的处理，每一条生成规则的实现都得以变得非常简洁。需要注意的是，我们没有必要为括号构造AST节点。虽然这么做也没错，但括号的作用主要还是对表达式进行分组进而引导语法解析过程。当语法解析器构造完AST之后，括号就没用了。

#### 变量引用和函数调用 规则解析

```c++
/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;         // 第一个token一定是个变量或者函数名
  getNextToken();  // eat identifier.

  if (CurTok != '(') // Simple variable ref.    // 第二个token如果不是左括号，很明显就是一个变量赋值.return.
    return new VariableExprAST(IdName);

  // Call.
  getNextToken();  // eat (                     // 吃左括号
  std::vector<ExprAST*> Args;                   // 左括号跟的一定是函数的参数列表，先做个参数数组放这里
  if (CurTok != ')') {                          // 在没有遇到右括号之前，一直递归while执行
    while (1) {
      ExprAST *Arg = ParseExpression();         // 参数以表达式的方式呈现，通过此函数获取一个表达式()
      if (!Arg) return 0;
      Args.push_back(Arg);

      if (CurTok == ')') break;                 // 做完当前参数可能遇到右括号，break

      if (CurTok != ',')                        // 做完当前参数，没有遇到有括号，就必须是","
        return Error("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }
```

该函数与其它函数的风格别无二致。（调用该函数时当前语元必须是`tok_identifier`。）前文提到的有关递归和错误处理的特点它统统具备。有意思的是这里采用了**预读**（lookahead）的手段来试探当前标识符的类型，判断它究竟是个独立的变量引用还是个函数调用。只要检查紧跟标识符之后的语元是不是“`(`”，它就能知道到底应该构造`VariableExprAST`节点还是`CallExprAST`节点。

现在，解析各种表达式的代码都已经完成，不妨再添加一个辅助函数，为它们梳理一个统一的入口。我们将上述表达式称为**主表达式**（primary expression。在解析各种主表达式时，我们首先要判定待解析表达式的类别。

#### 词法分析器引导函数

```c++
/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static ExprAST *ParsePrimary() {
  switch (CurTok) {
  default: return Error("unknown token when expecting an expression");
  case tok_identifier: return ParseIdentifierExpr();
  case tok_number:     return ParseNumberExpr();
  case '(':            return ParseParenExpr();
  }
}
```

看完这个函数的定义，你就能明白为什么先前的各个函数能够放心大胆地对`CurTok`的取值作出假设了。这里预读了下一个语元，预先对待解析表达式的类型作出了判断，然后才调用相应的函数进行解析。

### 第二章·解析二元表达式

基本表达式全都搞定了，下面开始开始着手解析更为复杂的二元表达式。

二元表达式的解析难度要大得多，因为它们往往具有二义性。例如，给定字符串“`x+y*z`”，语法解析器既可以将之解析为“`(x+y)*z`”，也可以将之解析为“`x+(y*z)`”。按照通常的数学定义，我们期望解析成后者，因为“`*`”（乘法）的优先级要高于“`+`”（加法）。

这个问题的解法很多，其中属[运算符优先级解析](http://en.wikipedia.org/wiki/Operator-precedence_parser)最为优雅和高效。这是一种利用二元运算符的优先级来引导递归调用走向的解析技术。

#### 运算符优先级表

首先，我们需要制定一张优先级表：

```c++
/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0) return -1;
  return TokPrec;
}

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;  // highest.
  ...
}
```

最基本的Kaleidoscope语言仅支持4种二元运算符（对于我们英勇无畏的读者来说，再加几个运算符自然是小菜一碟）。函数`GetTokPrecedence`用于查询当前语元的优先级，如果当前语元不是二元运算符则返回`-1`。这里的`map`简化了新运算符的添加，同时也可以证明我们的算法与具体的运算符无关。当然，要想去掉`map`直接在`GetTokPrecedence`中比较优先级也很简单。（甚至可以直接使用定长数组）。

#### RHS表达式解析

有了上面的函数作为辅助，我们就可以开始解析二元表达式了。**运算符优先级解析的基本思想就是通过拆解含有二元运算符的表达式来解决可能的二义性问题**。以表达式“`a+b+(c+d)*e*f+g`”为例，在进行运算符优先级解析时，它将被视作一串按**二元运算符**分隔的**主表达式**(primary expression)。按照这个思路，解析出来的第一个主表达式应该是“`a`”，紧跟着是若干个有序对，即：`[+, b]`、`[+, (c+d)]`、`[*, e]`、`[*, f]`和`[+, g]`。注意，**括号表达式也是主表达式**，所以在解析二元表达式时无须特殊照顾`(c+d)`这样的嵌套表达式。

有序对的格式为``[binop, primaryexpr]`` 包括一个运算符和一个主表达式，``LHS``是Right Hand Side的缩写，用于表示运算符左侧表达式，``RHS``同理表示运算符右侧表达式。

定义函数`ParseBinOpRHS`用于进一步解析有序对列表。它的入参包括一个整数和一个指针，其中整数代表运算符优先级(即上文中的``BinopPrecedence``)，指针则指向当前已解析出来的那部分表达式(LHS)。注意，单独一个“`x`”也是合法的表达式：也就是说`binoprhs`有可能为空(无运算符且无右表达式)。碰到这种情况时，函数将直接返回作为参数传入的表达式。

传入`ParseBinOpRHS`的优先级表示的是该函数所能处理的**最低运算符优先级**。假设函数当前处理的``RHS``语元流中的一对是“`[+, b]`”，且传入`ParseBinOpRHS`的优先级是`40`，那么该函数将直接返回（因为“`+`”的优先级是`20`）。搞清楚这一点之后，我们再来看`ParseBinOpRHS`的定义，函数的开头是这样的：

```c++
/// binoprhs
///   ::= ('+' primary)*
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
  // If this is a binop, find its precedence.
  while (1) {
    int TokPrec = GetTokPrecedence();       // 获取当前运算符优先级（使用curToken查找）

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
      
    // 如果token是一个已经定义的运算符，即可查找对对应的运算符优先级。
    // 如token果是一个没有定义的字符，则返回-1。  这字符可能是运算符也可能是一个主表达式。
    if (TokPrec < ExprPrec)   
      return LHS;
```

在二元运算符处理完毕（并保存妥当）之后，紧跟其后的主表达式也需进行对应的解析(如下示例)。至此，第一对有序对`[+, b]`就构造完了。

```c++
    // Okay, we know this is a binop.
    int BinOp = CurTok;
    getNextToken();  // eat binop

    // Parse the primary expression after the binary operator.
    ExprAST *RHS = ParsePrimary();
    if (!RHS) return 0;
```

#### LHS表达式解析和衔接

LHS是最为简单的，初始的LHS是一个单纯的变量\数值，通过``ParsePrimary``即可得到，进而将LHS表征的运算符优先级和指针作为入参传入`ParseBinOpRHS`。初始token流的入参优先级是0，LHS是``a``。

```c++
/// expression
///   ::= primary binoprhs
///
static ExprAST *ParseExpression() {
  ExprAST *LHS = ParsePrimary();
  if (!LHS) return 0;

  return ParseBinOpRHS(0, LHS);  // 运算符优先级默认为0
}
```

#### 表达式结合次序

现在表达式的左侧和`RHS`序列中第一对都已经解析完毕，该考虑表达式的结合次序了。路有两条，要么选择“`(a+b) binop unparse-part`”，要么选择“`a + (b binop unparse-part)`”。为了搞清楚到底该怎么走，我们在````ParseBinOpRHS``做了预读出“`binop`”，查出它的优先级，再将之与`BinOp`（本例中是“`+`”）的优先级相比较。(为什么是预读取呢？因为在``GetTokPrecedence``函数中并没有执行``getNextToken``操作，也就是说token没有走到下一跳。后面的操作还是在执行``binop``的token)

```c++
    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
```

`binop`位于“`RHS`”的右侧，如果`binop`的优先级低于或等于当前运算符的优先级，即按“`(a+b) binop ...`”处理。在本例中，当前运算符是“`+`”，下一个运算符也是“`+`”，二者的优先级相同。既然如此，理应按照“`a+b`”来构造AST节点，然后我们继续解析：

```c++
      // ... if body omitted ...
    }

    // Merge LHS/RHS.
    LHS = new BinaryExprAST(BinOp, LHS, RHS);       //LHS在不断的扩充膨胀
  }  // loop around to the top of the while loop.
}
```

接着上面的例子“`a+b+(c+d)*e*f+g`”，通过LHS和RHS解析我们已完成{a,[+,b]}的token流解析。接下来``[+ ,(c+d)]``“进入词法解析流程中来。``+``为当前语元，`(c+d)`”为主表达式。现在，主表达式右侧的`binop`是“`*`”，由于“`*`”的优先级高于“`+`”，负责检查运算符优先级的`if`判断通过，执行流程得以进入`if`语句的内部。

```c++
    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {                     // 为什么不是<=?   
      RHS = ParseBinOpRHS(TokPrec+1, RHS);        // 为什么要+1？    本质上也是为了获取比当前优先级更高的，哪怕是运算优先级相同也return LHS。
                                                  // 当然，我们需要尽可能贪婪的获取同优先级等级的主表达式，此方法通过while(1)实现.
      if (RHS == 0) return 0;
    }

    // Merge LHS/RHS.
    LHS = new BinaryExprAST(BinOp, LHS, RHS);
  }
}
```

看一下主表达式右侧的二元运算符，我们发现它的优先级比当前正在解析的`binop`的优先级要高。由此可知，如果自`binop`以右的若干个连续有序对都含有优先级高于“`+`”的运算符，那么就应该把它们全部解析出来，拼成“`RHS`”后返回。为此，我们将最低优先级设为“`TokPrec+1`”，递归调用函数“`ParseBinOpRHS`”。这样就可以递归的获取比当前更高优先级的表达式。

当然，看到这里您或许有疑问：同运算符优先级的注表达式难道就不递归获取了么？当然不是。只需要将整个``ParseBinOpRHS``函数内容通过while(1)封装起来，就能持续递归迭代了，其while循环终结条件是前面代码中有提到的内容(要么运算符优先级小，要么RHS==0)。

### 第二章·解析函数原型

下面来解析函数原型。在Kaleidoscope语言中，有两处会用到函数原型：一是“`extern`”函数声明，二是函数定义。

```c++
/// prototype
///   ::= id '(' id* ')'
static PrototypeAST *ParsePrototype() {
  if (CurTok != tok_identifier)
    return ErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;                 // 函数名称 字符串
  getNextToken();

  if (CurTok != '(')
    return ErrorP("Expected '(' in prototype");       // 函数后面必须有左括号

  std::vector<std::string> ArgNames;                  // 函数参数列表(看起来没有`,`的token的)
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return ErrorP("Expected ')' in prototype");

  // success.
  getNextToken();  // eat ')'.

  return new PrototypeAST(FnName, ArgNames);
}
```

在此基础之上，函数定义就很简单了，说白了就是一个函数原型再加一个用作函数体的表达式：

```c++
/// definition ::= 'def' prototype expression
static FunctionAST *ParseDefinition() {
  getNextToken();  // eat def.
  PrototypeAST *Proto = ParsePrototype();
  if (Proto == 0) return 0;

  if (ExprAST *E = ParseExpression())
    return new FunctionAST(Proto, E);
  return 0;
}
```

除了用于用户自定义函数的前置声明，“`extern`”语句还可以用来声明“`sin`”、“`cos`”等（C标准库）函数。这些“`extern`”语句就是些不带函数体的函数原型：

```c++
/// external ::= 'extern' prototype
static PrototypeAST *ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}
```

最后，我们还允许用户随时在顶层输入任意表达式并求值。这一特性是通过一个特殊的匿名零元函数（没有任何参数的函数）实现的，所有顶层表达式都定义在这个函数之内：

```c++
/// toplevelexpr ::= expression
static FunctionAST *ParseTopLevelExpr() {
  if (ExprAST *E = ParseExpression()) {
    // Make an anonymous proto.
    PrototypeAST *Proto = new PrototypeAST("", std::vector<std::string>());
    return new FunctionAST(Proto, E);
  }
  return 0;
}
```

现在所有零部件都准备完毕了，只需再编写一小段引导代码就可以跑起来了！

### 第二章·语法分析引导代码

引导代码很简单，只需在最外层的循环中按当前语元的类型选定相应的解析函数就可以了。这段实在没什么可介绍的，我就单独把最外层循环贴出来好了。

```c++
/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (1) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:    return;
    case ';':        getNextToken(); break;  // ignore top-level semicolons.
    case tok_def:    HandleDefinition(); break;
    case tok_extern: HandleExtern(); break;
    default:         HandleTopLevelExpression(); break;
    }
  }
}
```

这段代码最有意思的地方在于我们忽略了顶层的分号。为什么呢？举个例子，当你在命令行中键入“`4 + 5`”后，语法解析器无法判断你键入的内容是否已经完结。如果下一行键入的是“`def foo...`”，则可知顶层表达式就到`4+5`为止；但你也有可能会接着前面的表达式继续输入“`* 6`”。有了顶层的分号，你就可以输入“`4+5;`”，于是语法解析器就能够辨别表达式在何处结束了。

### 第二章·代码执行

算上注释我们一共只编写了不到400行代码（去掉注释和空行后只有240行），就完整地实现了包括词法分析器、语法解析器及AST生成器在内的最基本的Kaleidoscope语言。由此编译出的可执行文件用于校验Kaleidoscope代码在语法方面的正确性。

编译方法如下：

```bash
# Compile
clang++ -g -O3 toy.cpp
# Run
./a.out
```

代码运行示例：

```bash
$ ./a.out
ready> def foo(x y) x+foo(y, 4.0);
Parsed a function definition.
ready> def foo(x y) x+y y;
Parsed a function definition.
Parsed a top-level expr
ready> def foo(x y) x+y );
Parsed a function definition.
Error: unknown token when expecting an expression
ready> extern sin(a);
ready> Parsed an extern
ready> ^D
$
```

可扩展的地方还有很多。通过定义新的AST节点，你可以按各种方式对语言进行扩展。
