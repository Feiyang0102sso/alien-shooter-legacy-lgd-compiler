// as1（AS1 / Theseus）的 Legacy LGD（变体 A）写盘与回读校验。
//
// 包含两部分：
//   1. LOGIC::SaveAs：原版 LOGIC::Save() 的“指定输出路径”版本。
//      原版 Save() 只会写到源文件旁边（filename 去掉扩展名 + ".lgd"）。
//      输出目录与源目录不同时，那样会覆盖源目录里已有的 lgd，所以另加这个函数。
//      函数体与 vendor/logic.cpp 中的 LOGIC::Save() 逐行一致，只把路径换成参数。
//   2. LOGIC::SameAs + VerifyLegacyLgd：把写出的 lgd 用 LOGIC::Load() 读回，与编译结果逐项比较。
//
// 本文件包含精简版 runtime.h（会引入 vendor 头文件），因此只能使用 win32_abi.h 声明的 CRT 函数。

#include "runtime.h"
#include "lgd_writer.h"
#include "cli_frontend.h"

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

// 栈对象 flag：带初始值（会写出 Value）
static const unsigned char kStackFlagHasValue = 0x08;

// 校验时每类差异最多打印多少条
static const int kMaxReportedDiffs = 10;

static const int kMessageBufferSize = 1024;

// ---------------------------------------------------------------------------
// LOGIC 成员（vendor/logic.h 中新增的两个声明）
// ---------------------------------------------------------------------------

int LOGIC::SaveAs(const STRING* path)
{
    const int no=stack.No();
    FSTREAM file(path,"wb");
    if (!file.IsOpen()) {
        Error(3,const_cast<char*>("file (.lgd)"),0);
        return 1;
    }

    file.Write(&no,4u);
    SaveVar(&file);
    file.Write(&size,4u);
    file.Write(data,static_cast<unsigned int>(size));
    return 0;
}

// 只比较会被 SaveVar / SaveAs 写进文件、且有意义的内容：
//   栈对象：flag，带值时的字符串或整数；
//   符号：名字、类型、addr/code/def_value、begvar、no_var、def_str；
//   字节码：长度与内容。
// 符号记录里的填充字节和 def_str 指针值是内存残留，不参与比较。
// 返回差异条数，0 表示一致。
int LOGIC::SameAs(LOGIC* other)
{
    char message[kMessageBufferSize];
    int diffCount = 0;

    if (stack.m_no != other->stack.m_no) {
        sprintf(message, "[legacy_lgd] verify: stack count %d vs %d", stack.m_no, other->stack.m_no);
        CliLog(message);
        ++diffCount;
    } else {
        for (int i = 0; i < stack.m_no; ++i) {
            LOGICSTACK& left = stack.m_data[i];
            LOGICSTACK& right = other->stack.m_data[i];
            bool equal = left.type == right.type;
            if (equal && (left.type & kStackFlagHasValue)) {
                if (left.IsString()) {
                    equal = strcmp(left.string.m_buf, right.string.m_buf) == 0;
                } else {
                    equal = left.value == right.value;
                }
            }
            if (equal) {
                continue;
            }
            ++diffCount;
            if (diffCount <= kMaxReportedDiffs) {
                sprintf(message, "[legacy_lgd] verify: stack[%d] differs", i);
                CliLog(message);
            }
        }
    }

    if (var.m_no != other->var.m_no) {
        sprintf(message, "[legacy_lgd] verify: symbol count %d vs %d", var.m_no, other->var.m_no);
        CliLog(message);
        ++diffCount;
    } else {
        for (int i = 0; i < var.m_no; ++i) {
            NAMED_LIST_STRUCT<LOGICVAR>& left = var.m_data[i];
            NAMED_LIST_STRUCT<LOGICVAR>& right = other->var.m_data[i];
            bool equal = strcmp(left.name.m_buf, right.name.m_buf) == 0;
            if (equal) {
                equal = left.val.type == right.val.type
                    && left.val.addr == right.val.addr
                    && left.val.begvar == right.val.begvar
                    && left.val.no_var == right.val.no_var;
            }
            if (equal) {
                equal = strcmp(left.val.def_str.m_buf, right.val.def_str.m_buf) == 0;
            }
            if (equal) {
                continue;
            }
            ++diffCount;
            if (diffCount <= kMaxReportedDiffs) {
                sprintf(message, "[legacy_lgd] verify: symbol[%d] %s differs", i, left.name.m_buf);
                CliLog(message);
            }
        }
    }

    if (size != other->size) {
        sprintf(message, "[legacy_lgd] verify: bytecode size %d vs %d", size, other->size);
        CliLog(message);
        ++diffCount;
    } else {
        for (int i = 0; i < size; ++i) {
            if (data[i] != other->data[i]) {
                CliLog("[legacy_lgd] verify: bytecode content differs");
                ++diffCount;
                break;
            }
        }
    }

    return diffCount;
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------

bool WriteLegacyLgd(LOGIC& logic, const char* outputPath)
{
    STRING lgdName(outputPath);
    return logic.SaveAs(&lgdName) == 0;
}

bool VerifyLegacyLgd(LOGIC& compiled, const char* lgdPath)
{
    LOGIC reloaded;
    STRING lgdName(lgdPath);
    if (reloaded.Load(&lgdName) != 0) {
        char message[kMessageBufferSize];
        sprintf(message, "[legacy_lgd] verify: engine failed to load %s", lgdPath);
        CliLog(message);
        return false;
    }
    return compiled.SameAs(&reloaded) == 0;
}
