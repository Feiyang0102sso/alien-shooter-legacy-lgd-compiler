// Revisited / ZS2 一代的 Legacy LGD（变体 B）写盘与回读校验（与 as2_addon 的实现相同）。

#include "lgd_writer.h"

#include "script.h"
#include "stream.h"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

static const unsigned char kStackFlagString = 0x01;
static const unsigned char kStackFlagHasValue = 0x08;

// 变体 B 的符号记录长度：类型、标志、填充、值/偏移、栈起点、个数、源文件号、行号
static const unsigned int kSymbolRecordSize = 24;
static_assert(sizeof(SCRIPTVAR) == kSymbolRecordSize, "SCRIPTVAR must be 24 bytes; build as x86");

// 校验时每类差异最多打印多少条
static const int kMaxReportedDiffs = 10;

// ---------------------------------------------------------------------------
// 写盘
// ---------------------------------------------------------------------------

bool WriteLegacyLgd(const SCRIPT& script, const char* outputPath)
{
    // 以下与原版 SCRIPT::Save() 逐行一致，只是路径改为参数
    FSTREAM stream(STRING(outputPath), STRING("wb"));
    if (!stream.IsOpen()) {
        std::fprintf(stderr, "[legacy_lgd] cannot create %s\n", outputPath);
        return false;
    }
    stream.Write(&script.m_stack.m_count, 4u);
    script.SaveVar(&stream);
    stream.Write(&script.m_bytecodeCursor, 4u);
    stream.Write(script.m_bytecode, static_cast<unsigned int>(script.m_bytecodeCursor));
    return true;
}

// ---------------------------------------------------------------------------
// 回读校验
// ---------------------------------------------------------------------------

// 比较两个栈对象中会被序列化的部分
static bool StackEntryEqual(const SCRIPTSTACK& left, const SCRIPTSTACK& right)
{
    if (left.m_flags != right.m_flags) {
        return false;
    }
    if ((left.m_flags & kStackFlagHasValue) == 0) {
        return true;
    }
    if ((left.m_flags & kStackFlagString) != 0) {
        return std::strcmp(left.m_stringValue.m_value, right.m_stringValue.m_value) == 0;
    }
    return left.m_intValue == right.m_intValue;
}

// 比较两个符号中会被序列化的字段（变体 B 没有文本区，多了源文件号和行号）
static bool SymbolEqual(const NAMED_LIST_STRUCT<SCRIPTVAR>& left, const NAMED_LIST_STRUCT<SCRIPTVAR>& right)
{
    if (std::strcmp(left.m_name.m_value, right.m_name.m_value) != 0) {
        return false;
    }
    const SCRIPTVAR& a = left.m_value;
    const SCRIPTVAR& b = right.m_value;
    if (a.m_type != b.m_type || a.m_functionFlags != b.m_functionFlags) {
        return false;
    }
    if (a.m_valueOrOffset != b.m_valueOrOffset) {
        return false;
    }
    if (a.m_parameterStackBase != b.m_parameterStackBase) {
        return false;
    }
    if (a.m_elementOrParameterCount != b.m_elementOrParameterCount) {
        return false;
    }
    if (a.m_sourceFileIndex != b.m_sourceFileIndex || a.m_sourceLine != b.m_sourceLine) {
        return false;
    }
    return true;
}

bool VerifyLegacyLgd(const SCRIPT& compiled, const char* lgdPath)
{
    SCRIPT reloaded;
    int loadResult = reloaded.Load(STRING(lgdPath));
    if (loadResult != 0) {
        std::fprintf(stderr, "[legacy_lgd] verify: engine failed to load %s\n", lgdPath);
        return false;
    }

    int diffCount = 0;

    if (compiled.m_stack.m_count != reloaded.m_stack.m_count) {
        std::fprintf(stderr, "[legacy_lgd] verify: stack count %d vs %d\n",
                     compiled.m_stack.m_count, reloaded.m_stack.m_count);
        ++diffCount;
    } else {
        for (int i = 0; i < compiled.m_stack.m_count; ++i) {
            if (StackEntryEqual(compiled.m_stack.m_data[i], reloaded.m_stack.m_data[i])) {
                continue;
            }
            ++diffCount;
            if (diffCount <= kMaxReportedDiffs) {
                std::fprintf(stderr, "[legacy_lgd] verify: stack[%d] differs\n", i);
            }
        }
    }

    if (compiled.m_functionList.m_count != reloaded.m_functionList.m_count) {
        std::fprintf(stderr, "[legacy_lgd] verify: symbol count %d vs %d\n",
                     compiled.m_functionList.m_count, reloaded.m_functionList.m_count);
        ++diffCount;
    } else {
        for (int i = 0; i < compiled.m_functionList.m_count; ++i) {
            if (SymbolEqual(compiled.m_functionList.Record(i), reloaded.m_functionList.Record(i))) {
                continue;
            }
            ++diffCount;
            if (diffCount <= kMaxReportedDiffs) {
                std::fprintf(stderr, "[legacy_lgd] verify: symbol[%d] %s differs\n",
                             i, compiled.m_functionList.Record(i).m_name.m_value);
            }
        }
    }

    if (compiled.m_bytecodeCursor != reloaded.m_bytecodeCursor) {
        std::fprintf(stderr, "[legacy_lgd] verify: bytecode size %d vs %d\n",
                     compiled.m_bytecodeCursor, reloaded.m_bytecodeCursor);
        ++diffCount;
    } else if (compiled.m_bytecodeCursor > 0) {
        if (std::memcmp(compiled.m_bytecode, reloaded.m_bytecode,
                        static_cast<size_t>(compiled.m_bytecodeCursor)) != 0) {
            std::fprintf(stderr, "[legacy_lgd] verify: bytecode content differs\n");
            ++diffCount;
        }
    }

    return diffCount == 0;
}
