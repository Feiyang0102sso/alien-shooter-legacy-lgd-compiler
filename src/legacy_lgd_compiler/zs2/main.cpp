// legacy_lgd_compiler_zs2：用 ZS2 地图编辑器的编译器（SCRIPT），把 .lgc 编译成 Legacy LGD（变体 B）。
// 目标引擎：Alien Shooter: Revisited（ASR）/ Zombie Shooter 2（ZS2）。
//
// 命令行参数由 common/cli_frontend 统一处理，与官方 lgc_compiler.exe 一致。
//
// 写完后用引擎自己的 Load() 读回校验，读回结果与编译结果不一致视为失败。
//
// 与 as2_addon 的唯一区别：iff 编译成 8 字节操作数（跳转偏移 + 条件起点），AS2C / AS2R 是 4 字节，不要混用。
//
// 提醒：这一代引擎从 lgd 加载时不会补算符号哈希，按名查找全部失败（连 main 都找不到），
//       产物在游戏里跑不起来，仅供研究（详见 legacy_lgd_doc_CN.md）。

#include "script.h"
#include "lgd_writer.h"
#include "cli_frontend.h"

#include <cstdio>

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

static const char* const kToolName = "legacy_lgd_compiler_zs2";

// ---------------------------------------------------------------------------

// 编译一个脚本、写出 lgd 并回读校验；成功返回 true
static bool CompileZs2(const char* sourcePath, const char* outputPath)
{
    SCRIPT script;
    int loadResult = script.Load(STRING(sourcePath));
    if (loadResult != 0 || script.m_errorLatch != 0) {
        return false;
    }

    if (!WriteLegacyLgd(script, outputPath)) {
        return false;
    }

    if (!VerifyLegacyLgd(script, outputPath)) {
        std::fprintf(stderr, "[%s] read-back verification failed: %s\n", kToolName, outputPath);
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    MYERROR errorLogger(stderr);
    Error = &errorLogger;
    return RunCompilerCli(argc, argv, kToolName, CompileZs2);
}
