#pragma once
// stubs.cpp 对外暴露的少量接口：编译错误计数。
// AS1 的编译器出错时不置任何标志，只能靠统计 MYERROR::Error 的调用次数判断编译是否成功。

// 自上次重置以来 MYERROR::Error 报告的错误数（不含 LOGIC 的上下文行）
int CliErrorCount();

// 编译下一个文件前清零
void CliResetErrorCount();
