// AS2 原版的 Legacy LGD（变体 A）写盘与回读校验。

#include "lgd_writer.h"

#include "script.h"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

static const unsigned char kStackFlagString = 0x01;
static const unsigned char kStackFlagHasValue = 0x08;
static const unsigned char kStackStringXorKey = 0x17;

// 变体 A 的符号记录长度。写盘方式与零售版一致：把内存里的结构体原样写出，
// 其中 m_text 的 STRING 指针也会被写进去（读取时会被文本区覆盖，属于垃圾字段）。
static const unsigned int kSymbolRecordSize = 20;
static_assert(sizeof(SCRIPTVAR) == kSymbolRecordSize, "SCRIPTVAR must be 20 bytes; build as x86");

// 校验时每类差异最多打印多少条
static const int kMaxReportedDiffs = 10;

// ---------------------------------------------------------------------------
// 写盘
// ---------------------------------------------------------------------------

// 写一个以 \0 结尾的字符串
static void WriteCString(std::FILE* file, const char* text)
{
    std::fwrite(text, 1, std::strlen(text) + 1, file);
}

// 写一个栈对象：与 SCRIPTSTACK::Read 互逆
static void WriteStackEntry(std::FILE* file, const SCRIPTSTACK& entry)
{
    std::fwrite(&entry.m_flags, 1, 1, file);
    if ((entry.m_flags & kStackFlagHasValue) == 0) {
        return;
    }

    if ((entry.m_flags & kStackFlagString) != 0) {
        const char* text = entry.m_stringValue.m_value;
        size_t length = std::strlen(text);
        for (size_t i = 0; i < length; ++i) {
            unsigned char encoded = static_cast<unsigned char>(text[i]) ^ kStackStringXorKey;
            std::fwrite(&encoded, 1, 1, file);
        }
        unsigned char terminator = 0;
        std::fwrite(&terminator, 1, 1, file);
        return;
    }

    std::fwrite(&entry.m_intValue, 4, 1, file);
}

bool WriteLegacyLgd(const SCRIPT& script, const char* outputPath)
{
    std::FILE* file = std::fopen(outputPath, "wb");
    if (!file) {
        std::fprintf(stderr, "[legacy_lgd] cannot create %s\n", outputPath);
        return false;
    }

    // 头：栈对象数量
    unsigned int stackCount = static_cast<unsigned int>(script.m_stack.m_count);
    std::fwrite(&stackCount, 4, 1, file);

    // 第一部分：栈对象表
    for (int i = 0; i < script.m_stack.m_count; ++i) {
        WriteStackEntry(file, script.m_stack.m_data[i]);
    }

    // 第二部分：符号表（名字 + 20 字节原始记录）
    int symbolCount = script.m_functionList.m_count;
    std::fwrite(&symbolCount, 4, 1, file);
    for (int i = 0; i < symbolCount; ++i) {
        const NAMED_LIST_STRUCT<SCRIPTVAR>& record = script.m_functionList.Record(i);
        WriteCString(file, record.m_name.m_value);
        std::fwrite(&record.m_value, kSymbolRecordSize, 1, file);
    }

    // 文本区：每个符号的 m_text
    for (int i = 0; i < symbolCount; ++i) {
        const NAMED_LIST_STRUCT<SCRIPTVAR>& record = script.m_functionList.Record(i);
        WriteCString(file, record.m_value.m_text.m_value);
    }

    // 第三部分：字节码
    unsigned int bytecodeSize = static_cast<unsigned int>(script.m_bytecodeCursor);
    std::fwrite(&bytecodeSize, 4, 1, file);
    if (bytecodeSize > 0) {
        std::fwrite(script.m_bytecode, 1, bytecodeSize, file);
    }

    bool writeOk = std::ferror(file) == 0;
    std::fclose(file);
    if (!writeOk) {
        std::fprintf(stderr, "[legacy_lgd] write error on %s\n", outputPath);
    }
    return writeOk;
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

// 比较两个符号中会被序列化、且有意义的字段（m_text 指针本身不比较，比较其内容）
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
    return std::strcmp(a.m_text.m_value, b.m_text.m_value) == 0;
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
