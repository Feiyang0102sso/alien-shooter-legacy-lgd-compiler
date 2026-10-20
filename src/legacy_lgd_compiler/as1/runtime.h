#pragma once
// 精简版 runtime.h（替代 AS1 地图编辑器原工程的总头文件 runtime.hpp）。
//
// 原工程的 runtime.hpp 会把渲染、音频、地图等全部模块按 ABI 顺序拉进来；
// 编译器（LOGIC）只需要其中的字符串、容器、流和错误日志。
// 这里只按原顺序引入这几个模块，其余模块不参与编译。
//
// 注意：原工程不引入任何系统头文件（<stdio.h> 等），CRT 函数全部由
// win32_abi.h 自行声明（例如 fopen 返回 void*）。凡是包含本头文件的
// 编译单元都不能再引入 <stdio.h>；需要 stdio 的代码放在 common/cli_frontend.cpp。

#include "runtime_foundation.h"
#include "win32_abi.h"
#include "mystring.h"
#include "containers.h"
#include "stream.h"
#include "logic.h"
#include "myerror.h"

// logic.cpp 里 ScriptExecFunc 会转发到 Map->ExecFunc。
// 编译期不会执行 extern 函数，这里只提供一个最小定义让它能链接。
class MAP {
public:
    int ExecFunc(int command);
};

// globals.h 引用的其余编辑器类型，编译器用不到，前向声明即可
class GRAPH;
class REGISTRY;
class PROFILE;
struct GRAPH_INIT;
class CONSTANT;

#include "globals.h"
