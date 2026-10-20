// legacy_lgd_compiler_as1：用 AS1 地图编辑器的原版编译器（LOGIC），把 .lgc 编译成 Legacy LGD（变体 A）。
// 目标引擎：AS1 / ZS1 / Theseus 一代。
//
// 命令行参数由 common/cli_frontend 统一处理，与官方 lgc_compiler.exe 一致。
//
// 写完后用引擎自己的 Load() 读回校验，读回结果与编译结果不一致视为失败。
//
// 本文件包含精简版 runtime.h（会引入 vendor 头文件），因此不能引入 <stdio.h>。

#include "runtime.h"
#include "lgd_writer.h"
#include "cli_frontend.h"
#include "stubs.h"

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

static const char* const kToolName = "legacy_lgd_compiler_as1";
static const int kMessageBufferSize = 1024;

// ---------------------------------------------------------------------------

// 编译一个脚本、写出 lgd 并回读校验；成功返回 true
static bool CompileAs1(const char* sourcePath, const char* outputPath)
{
    CliResetErrorCount();

    LOGIC logic;
    STRING scriptName(sourcePath);
    int loadResult = logic.Load(&scriptName);
    if (loadResult != 0 || CliErrorCount() > 0) {
        return false;
    }

    if (!WriteLegacyLgd(logic, outputPath)) {
        return false;
    }

    if (!VerifyLegacyLgd(logic, outputPath)) {
        char message[kMessageBufferSize];
        sprintf(message, "[%s] read-back verification failed: %s", kToolName, outputPath);
        CliLog(message);
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    MYERROR errorLogger(0);
    Error = &errorLogger;
    return RunCompilerCli(argc, argv, kToolName, CompileAs1);
}
