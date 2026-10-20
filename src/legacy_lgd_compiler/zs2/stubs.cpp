// SCRIPT.CPP 依赖、但编译期用不到的编辑器函数：给出最小实现让它能链接。

#include <cstdio>

// MYERROR::Error 在报错信息里打印的游戏时钟；命令行下没有游戏循环，恒为 0
unsigned long CurrentTime = 0;

// 执行 extern 函数的分发入口。编译期不会执行脚本，走到这里说明调用方式不对。
int __cdecl ScriptExecFunc(int command)
{
    std::fprintf(stderr, "[legacy_lgd] unexpected ScriptExecFunc(%d) during compile\n", command);
    return 0;
}

// 编辑器在编译过程中刷新“加载中”画面用的回调，命令行下什么都不做
void __cdecl DispatchMapPresentation(int mode)
{
    (void)mode;
}
