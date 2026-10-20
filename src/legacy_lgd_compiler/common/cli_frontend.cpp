// 各编译目标共用的命令行前端。
//
// 行为与官方 lgc_compiler.exe 对齐（已实测）：
//   * 不给 FILES 时，递归扫描 --source-dir，只编译“同目录下有同名 .map”的 .lgc；
//   * 给了 FILES 时，路径相对于当前目录，不检查 .map；
//   * 输出到 --target-dir，保留相对于 --source-dir 的目录结构，扩展名换成 .lgd；
//   * --source-dir 默认为当前目录，--target-dir 默认与 --source-dir 相同；
//   * 每个文件输出一行 "Compile: <相对路径> [ OK ]"。
//
// 与官方的区别：
//   * 老引擎的 #include 相对于“游戏根目录”解析（脚本里写 "maps\common.lgc"），
//     所以编译前会把工作目录切到游戏根目录。默认取 --source-dir 的上一级，
//     也可以用 --game-root=DIR 指定。
//   * 老引擎编译器没有宏注入和包含路径机制，-D / -I 会被忽略并给出提示；-v 也被忽略。
//
// 子进程隔离：
//   原版编译器遇到语法错误会直接 exit(1)（AS1 有 43 处，AS2 有 4 处），而且内部有全局状态。
//   为了让一个文件出错不影响后面的文件，每个文件都由本程序以内部参数
//   "--compile-one <源文件> <输出文件>" 重新启动一个子进程来编译。

#include "cli_frontend.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

static const char* const kSourceDirOption = "--source-dir=";
static const char* const kTargetDirOption = "--target-dir=";
static const char* const kGameRootOption = "--game-root=";
static const char* const kStopOnErrorOption = "--stop-on-error";
static const char* const kVerboseShortOption = "-v";
static const char* const kVerboseLongOption = "--verbose";
static const char* const kHelpShortOption = "-h";
static const char* const kHelpLongOption = "--help";
static const char* const kDefineOptionPrefix = "-D";
static const char* const kIncludeOptionPrefix = "-I";

// 内部参数：只编译一个文件（由父进程启动子进程时使用，用户不需要直接调用）
static const char* const kCompileOneOption = "--compile-one";
static const int kCompileOneArgCount = 4;  // exe --compile-one <源文件> <输出文件>

static const char* const kScriptExtension = ".lgc";
static const char* const kMapExtension = ".map";
static const char* const kOutputExtension = ".lgd";

// 引擎判断源码 / 二进制的依据：首个 dword 的最高字节非 0 即视为文本源码
static const unsigned int kTextSourceMask = 0xFF000000u;

static const int kExitOk = 0;
static const int kExitFailed = 1;
static const int kExitUsage = 2;

// ---------------------------------------------------------------------------
// 数据结构
// ---------------------------------------------------------------------------

// 命令行选项
struct CliOptions {
    fs::path sourceDir;
    fs::path targetDir;
    fs::path gameRoot;
    bool hasTargetDir = false;
    bool hasGameRoot = false;
    bool stopOnError = false;
    std::vector<std::string> files;
};

// 一个编译任务
struct CompileJob {
    fs::path sourcePath;      // 绝对路径
    fs::path outputPath;      // 绝对路径
    std::string displayName;  // 日志里显示的相对路径（用 '/' 分隔，与官方一致）
};

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------

void CliLog(const char* text)
{
    std::fputs(text, stderr);
    std::fputc('\n', stderr);
}

static bool StartsWith(const std::string& text, const char* prefix)
{
    return text.rfind(prefix, 0) == 0;
}

// 扩展名比较不区分大小写（官方样本里 .LGC / .lgc 混用）
static bool HasExtension(const fs::path& path, const char* extension)
{
    std::string actual = path.extension().string();
    std::string expected(extension);
    if (actual.size() != expected.size()) {
        return false;
    }
    for (size_t i = 0; i < actual.size(); ++i) {
        char left = static_cast<char>(std::tolower(static_cast<unsigned char>(actual[i])));
        char right = static_cast<char>(std::tolower(static_cast<unsigned char>(expected[i])));
        if (left != right) {
            return false;
        }
    }
    return true;
}

// 与引擎相同的判定：首个 dword 最高字节非 0 才是文本源码；文件不存在也返回 false
static bool IsTextSource(const fs::path& path)
{
    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    if (!file) {
        return false;
    }
    unsigned int firstDword = 0;
    size_t readCount = std::fread(&firstDword, 1, sizeof(firstDword), file);
    std::fclose(file);
    if (readCount != sizeof(firstDword)) {
        return false;
    }
    return (firstDword & kTextSourceMask) != 0;
}

// 相对于 baseDir 的显示名；不在 baseDir 之下时只显示文件名
static std::string MakeDisplayName(const fs::path& path, const fs::path& baseDir)
{
    std::error_code error;
    fs::path relative = fs::relative(path, baseDir, error);
    if (error || relative.empty() || StartsWith(relative.string(), "..")) {
        return path.filename().generic_string();
    }
    return relative.generic_string();
}

// 输出路径：target-dir + 相对于 source-dir 的路径，扩展名换成 .lgd
static fs::path MakeOutputPath(const fs::path& sourcePath, const CliOptions& options)
{
    std::error_code error;
    fs::path relative = fs::relative(sourcePath, options.sourceDir, error);
    if (error || relative.empty() || StartsWith(relative.string(), "..")) {
        relative = sourcePath.filename();
    }
    relative.replace_extension(kOutputExtension);
    return options.targetDir / relative;
}

static void PrintUsage(const char* toolName)
{
    std::fprintf(stderr,
        "usage: %s [--source-dir=DIR] [--target-dir=DIR] [--game-root=DIR] [--stop-on-error] [FILES...]\n"
        "  --source-dir=DIR   where to look for .lgc (default: current directory)\n"
        "  --target-dir=DIR   where to write .lgd, keeping sub-directories (default: same as source)\n"
        "  --game-root=DIR    directory that #include \"maps\\...\" is relative to (default: parent of source)\n"
        "  --stop-on-error    stop at the first failed file\n"
        "  FILES              compile only these files (no .map check); otherwise every .lgc with a same-name .map\n",
        toolName);
}

// ---------------------------------------------------------------------------
// 参数解析
// ---------------------------------------------------------------------------

// 解析成功返回 true；遇到 -h / 未知选项时返回 false
static bool ParseOptions(int argc, char** argv, const char* toolName, CliOptions* options)
{
    options->sourceDir = fs::current_path();

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == kHelpShortOption || arg == kHelpLongOption) {
            PrintUsage(toolName);
            return false;
        }
        if (StartsWith(arg, kSourceDirOption)) {
            options->sourceDir = fs::path(arg.substr(std::strlen(kSourceDirOption)));
            continue;
        }
        if (StartsWith(arg, kTargetDirOption)) {
            options->targetDir = fs::path(arg.substr(std::strlen(kTargetDirOption)));
            options->hasTargetDir = true;
            continue;
        }
        if (StartsWith(arg, kGameRootOption)) {
            options->gameRoot = fs::path(arg.substr(std::strlen(kGameRootOption)));
            options->hasGameRoot = true;
            continue;
        }
        if (arg == kStopOnErrorOption) {
            options->stopOnError = true;
            continue;
        }
        if (arg == kVerboseShortOption || arg == kVerboseLongOption) {
            continue;
        }
        if (StartsWith(arg, kDefineOptionPrefix) || StartsWith(arg, kIncludeOptionPrefix)) {
            std::fprintf(stderr, "[%s] ignored '%s': legacy compiler has no -D / -I support\n", toolName, arg.c_str());
            continue;
        }
        if (StartsWith(arg, "-")) {
            std::fprintf(stderr, "[%s] unknown option: %s\n", toolName, arg.c_str());
            PrintUsage(toolName);
            return false;
        }
        options->files.push_back(arg);
    }

    // 统一转成绝对路径，之后会切换工作目录
    options->sourceDir = fs::absolute(options->sourceDir).lexically_normal();
    if (options->hasTargetDir) {
        options->targetDir = fs::absolute(options->targetDir).lexically_normal();
    } else {
        options->targetDir = options->sourceDir;
    }
    if (options->hasGameRoot) {
        options->gameRoot = fs::absolute(options->gameRoot).lexically_normal();
    } else {
        options->gameRoot = options->sourceDir.parent_path();
    }
    return true;
}

// ---------------------------------------------------------------------------
// 收集任务
// ---------------------------------------------------------------------------

// 递归扫描 source-dir：只收集同目录下有同名 .map 的 .lgc
static std::vector<fs::path> ScanMapScripts(const fs::path& sourceDir)
{
    std::vector<fs::path> scripts;
    std::error_code error;
    fs::recursive_directory_iterator it(sourceDir, error);
    fs::recursive_directory_iterator end;
    while (!error && it != end) {
        const fs::directory_entry& entry = *it;
        if (entry.is_regular_file() && HasExtension(entry.path(), kScriptExtension)) {
            fs::path mapPath = entry.path();
            mapPath.replace_extension(kMapExtension);
            // Windows 文件系统不区分大小写，LEVEL01.LGC 能匹配到 level01.map
            if (fs::exists(mapPath)) {
                scripts.push_back(entry.path());
            }
        }
        it.increment(error);
    }
    std::sort(scripts.begin(), scripts.end());
    return scripts;
}

static std::vector<CompileJob> CollectJobs(const CliOptions& options)
{
    std::vector<fs::path> sources;
    if (options.files.empty()) {
        sources = ScanMapScripts(options.sourceDir);
    } else {
        for (const std::string& file : options.files) {
            sources.push_back(fs::absolute(fs::path(file)).lexically_normal());
        }
    }

    std::vector<CompileJob> jobs;
    for (const fs::path& source : sources) {
        CompileJob job;
        job.sourcePath = source;
        job.outputPath = MakeOutputPath(source, options);
        job.displayName = MakeDisplayName(source, options.sourceDir);
        jobs.push_back(job);
    }
    return jobs;
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------

// 给命令行参数加引号（路径里可能有空格；这里的路径都是文件路径，不会以反斜杠结尾）
static std::string QuoteArgument(const std::string& text)
{
    return "\"" + text + "\"";
}

// 以 "--compile-one <源文件> <输出文件>" 启动本程序的子进程，返回子进程退出码；
// 无法启动子进程时返回 -1。子进程继承当前工作目录（游戏根目录）和标准输入输出。
static int RunCompileChild(const std::string& sourceText, const std::string& outputText)
{
    char exePath[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return -1;
    }

    std::string commandLine = QuoteArgument(exePath);
    commandLine += " ";
    commandLine += kCompileOneOption;
    commandLine += " ";
    commandLine += QuoteArgument(sourceText);
    commandLine += " ";
    commandLine += QuoteArgument(outputText);

    // CreateProcessA 可能改写命令行缓冲区，必须传可写内存
    std::vector<char> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back('\0');

    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION process = {};
    BOOL created = CreateProcessA(exePath, commandBuffer.data(), NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process);
    if (!created) {
        return -1;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = static_cast<DWORD>(kExitFailed);
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}

// 编译单个任务（在子进程里）；成功返回 true
static bool RunJob(const CompileJob& job, const char* toolName)
{
    if (!IsTextSource(job.sourcePath)) {
        std::fprintf(stderr, "[%s] not a text source (or missing): %s\n", toolName, job.sourcePath.string().c_str());
        return false;
    }

    std::error_code error;
    fs::create_directories(job.outputPath.parent_path(), error);
    if (error) {
        std::fprintf(stderr, "[%s] cannot create directory: %s\n", toolName,
                     job.outputPath.parent_path().string().c_str());
        return false;
    }

    // 子进程输出的编译日志要排在本行状态之前，先把已有输出刷出去
    std::fflush(stdout);
    std::fflush(stderr);

    int exitCode = RunCompileChild(job.sourcePath.string(), job.outputPath.string());
    if (exitCode == -1) {
        std::fprintf(stderr, "[%s] cannot start compiler process\n", toolName);
        return false;
    }
    if (exitCode != kExitOk && exitCode != kExitFailed) {
        // 既不是成功也不是正常失败：编译器崩溃了
        std::fprintf(stderr, "[%s] compiler process crashed (exit code 0x%08X)\n",
                     toolName, static_cast<unsigned int>(exitCode));
    }
    return exitCode == kExitOk;
}

int RunCompilerCli(int argc, char** argv, const char* toolName, CompileScriptFn compileScript)
{
    // 子进程模式：只编译一个文件，退出码告诉父进程结果
    if (argc == kCompileOneArgCount && std::strcmp(argv[1], kCompileOneOption) == 0) {
        bool ok = compileScript(argv[2], argv[3]);
        if (ok) {
            return kExitOk;
        }
        return kExitFailed;
    }

    CliOptions options;
    if (!ParseOptions(argc, argv, toolName, &options)) {
        return kExitUsage;
    }

    if (!fs::is_directory(options.sourceDir)) {
        std::fprintf(stderr, "[%s] source directory not found: %s\n", toolName, options.sourceDir.string().c_str());
        return kExitUsage;
    }

    std::vector<CompileJob> jobs = CollectJobs(options);
    if (jobs.empty()) {
        std::fprintf(stderr, "[%s] nothing to compile in %s (no .lgc with a same-name .map)\n",
                     toolName, options.sourceDir.string().c_str());
        return kExitOk;
    }

    // 老引擎的 #include 相对于游戏根目录解析
    std::error_code error;
    fs::current_path(options.gameRoot, error);
    if (error) {
        std::fprintf(stderr, "[%s] cannot enter game root: %s\n", toolName, options.gameRoot.string().c_str());
        return kExitUsage;
    }

    int okCount = 0;
    int failedCount = 0;
    for (const CompileJob& job : jobs) {
        bool ok = RunJob(job, toolName);
        if (ok) {
            ++okCount;
            std::fprintf(stdout, "Compile: %s [ OK ]\n", job.displayName.c_str());
        } else {
            ++failedCount;
            std::fprintf(stdout, "Compile: %s [ FAILED ]\n", job.displayName.c_str());
            if (options.stopOnError) {
                break;
            }
        }
        std::fflush(stdout);
    }

    std::fprintf(stdout, "[%s] %d compiled, %d failed\n", toolName, okCount, failedCount);
    if (failedCount > 0) {
        return kExitFailed;
    }
    return kExitOk;
}
