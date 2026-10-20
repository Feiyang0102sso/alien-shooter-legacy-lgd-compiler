#pragma once
// 各编译目标（as1 / as2 / ...）共用的命令行前端。
//
// 参数与官方 lgc_compiler.exe 保持一致：
//   <tool> [--source-dir=DIR] [--target-dir=DIR] [--stop-on-error] [FILES...]
// 另外多一个可选的 --game-root=DIR，见 cli_frontend.cpp 顶部说明。
//
// 本头文件不依赖任何 CRT / STL 类型，可以和 vendor 的头文件一起包含
// （as1 的 vendor 代码自行声明 CRT 函数，不能与 <stdio.h> 同时出现）。

// 编译一个脚本。两个参数都是绝对路径；调用时进程工作目录已切到游戏根目录。
// 成功（且已写出 outputPath）返回 true。
typedef bool (*CompileScriptFn)(const char* sourcePath, const char* outputPath);

// 解析参数、收集要编译的文件并逐个调用 compileScript，返回进程退出码
int RunCompilerCli(int argc, char** argv, const char* toolName, CompileScriptFn compileScript);

// 输出一行到 stderr（自动补换行）；供不能使用 <stdio.h> 的后端打印日志
void CliLog(const char* text);
