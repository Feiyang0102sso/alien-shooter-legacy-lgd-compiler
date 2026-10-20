// vendor 代码依赖、但没有一起复制过来的那部分实现（编辑器里它们依赖窗口 / 资源系统）。
// 与 as2 / as2_addon 的 stubs.cpp 职责相同；AS1 的 vendor 缺得更多，所以这里内容也更多。
//
// 包含三块：
//   1. STREAM / FSTREAM / FOpen：照搬零售版 resource_ini.cpp 的实现（fopen 级别）。
//   2. MYERROR：日志改为输出到 stderr，并统计错误次数，供 main 判断编译是否成功。
//   3. 全局对象 Map / Error，以及编译期永远不会被调用的 MAP::ExecFunc。
//
// 本文件包含精简版 runtime.h（会引入 vendor 头文件），因此只能使用 win32_abi.h 声明的 CRT 函数。

#include "runtime.h"
#include "cli_frontend.h"
#include "stubs.h"

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

// MYERROR::Error 的 type -> 格式串，顺序与零售版 MapEdit.exe 的跳转表一致
static const char* const kErrorFormats[15] = {
    "0x%X Couldn't lock %s",                  // 0  E_LOCK
    "0x%X Couldn't copy %s",                  // 1  E_COPY
    "%i There was not enough memory for %s",  // 2  E_MEMORY
    "0x%X Couldn't create the %s",            // 3  E_CREATE
    "0x%X Invalid %s",                        // 4  E_INVALID
    "0x%X Load %s",                           // 5  E_LOAD
    "0x%X Save %s",                           // 6  E_SAVE
    "0x%X Couldn't open '%s'",                // 7  E_OPEN
    "0x%X Couldn't set the %s",               // 8  E_SET
    "0x%X Couldn't get the %s",               // 9  E_GET
    "%i %s",                                  // 10 E_ERROR
    "0x%X Section can't found (%s)",          // 11 E_SECTION
    "0x%X Unable initialize %s",              // 12 E_INIT
    "%i Missing %s",                          // 13 E_MISSING
    "%i Unknownn %s"                          // 14 E_UNKNOWN
};
static const int kErrorFormatCount = 15;

// LOGIC::Error 每次报错后会额外打两行“上下文 + 箭头”，type 固定为 E_ERROR(10)、
// module 固定为 "LOGIC"。这两行属于同一个错误，不应重复计数。
static const int kContextErrorType = 10;
static const char* const kContextModule = "LOGIC";

static const int kMessageBufferSize = 4096;

// ---------------------------------------------------------------------------
// 全局对象
// ---------------------------------------------------------------------------

MAP* Map = 0;
MYERROR* Error = 0;

static int g_errorCount = 0;

int CliErrorCount()
{
    return g_errorCount;
}

void CliResetErrorCount()
{
    g_errorCount = 0;
}

int MAP::ExecFunc(int command)
{
    // 编译期不会执行任何 extern 函数；走到这里说明调用方式不对
    char message[kMessageBufferSize];
    sprintf(message, "[legacy_lgd] unexpected ExecFunc(%i) during compile", command);
    CliLog(message);
    return 0;
}

// ---------------------------------------------------------------------------
// STREAM / FSTREAM / FOpen（照搬零售版）
// ---------------------------------------------------------------------------

STREAM::STREAM() {}
STREAM::~STREAM() {}

FILE* FOpen(const STRING* name, const char* mode)
{
    if (name->m_buf[0] == 0) {
        return 0;
    }
    return static_cast<FILE*>(fopen(name->m_buf, mode));
}

FSTREAM::FSTREAM(const STRING* name, const char* mode) : file(FOpen(name, mode)) {}

FSTREAM::~FSTREAM()
{
    if (file) {
        fclose(file);
    }
}

int FSTREAM::IsOpen()
{
    return file != 0;
}

int FSTREAM::IsEnd()
{
    // 零售版返回 FILE::_flag & 0x10，这里用 feof 等价表达
    if (file && feof(file)) {
        return 0x10;
    }
    return 0;
}

// 注意：零售版的 Read / Write 返回的是“没读 / 没写成功的字节数”，成功时为 0
int FSTREAM::Read(void* data, unsigned int size)
{
    return (int)(size - fread(data, 1u, size, file));
}

int FSTREAM::Write(const void* data, unsigned int size)
{
    return (int)(size - fwrite(data, 1u, size, file));
}

int FSTREAM::Length()
{
    return (int)_filelength(_fileno(file));
}

// ---------------------------------------------------------------------------
// MYERROR（输出到 stderr）
// ---------------------------------------------------------------------------

// 把 printf 风格的参数格式化后输出一行
static void LogFormatted(const char* format, va_list args)
{
    char message[kMessageBufferSize];
    vsprintf(message, format, args);
    CliLog(message);
}

MYERROR::MYERROR(int clear_log_file) : hwnd(0), file(0)
{
    (void)clear_log_file;
    filename[0] = 0;
}

MYERROR::~MYERROR() {}

void __cdecl MYERROR::Log(const MYERROR* self, const char* str, ...)
{
    (void)self;
    va_list args;
    va_start(args, str);
    LogFormatted(str, args);
    va_end(args);
}

void __cdecl MYERROR::LogTmp(const MYERROR* self, const char* str, ...)
{
    (void)self;
    va_list args;
    va_start(args, str);
    LogFormatted(str, args);
    va_end(args);
}

void __cdecl MYERROR::LogExit(const MYERROR* self, const char* str, ...)
{
    (void)self;
    va_list args;
    va_start(args, str);
    LogFormatted(str, args);
    va_end(args);
    exit(1);
}

void __cdecl MYERROR::WindowExit(const char* str, ...)
{
    va_list args;
    va_start(args, str);
    LogFormatted(str, args);
    va_end(args);
    exit(1);
}

int __cdecl MYERROR::Window(const char* text, ...)
{
    va_list args;
    va_start(args, text);
    LogFormatted(text, args);
    va_end(args);
    return 0;
}

long MYERROR::FilterExcept(_EXCEPTION_RECORD* info)
{
    (void)info;
    return 0;
}

// 与零售版逻辑一致：先把 module 当格式串展开，再拼上 type 对应的格式串
void __cdecl MYERROR::Error(const MYERROR* self, const char* module, int type,
                            const char* text, unsigned long err, ...)
{
    char message[kMessageBufferSize];
    strcpy(message, "!!!ERROR!!!");

    va_list args;
    va_start(args, err);
    vsprintf(message + strlen(message), module, args);
    va_end(args);

    strcat(message, ": ");
    if (type >= 0 && type < kErrorFormatCount) {
        strcat(message, kErrorFormats[type]);
    }

    // 上下文行不计数，其余每次调用算一个错误
    bool isContextLine = false;
    if (type == kContextErrorType && strcmp(module, kContextModule) == 0) {
        isContextLine = true;
    }
    if (!isContextLine) {
        ++g_errorCount;
    }

    Log(self, message, err, text);
}
