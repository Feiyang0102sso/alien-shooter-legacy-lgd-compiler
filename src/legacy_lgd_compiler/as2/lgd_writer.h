#pragma once
// AS2 原版的 Legacy LGD（变体 A）写盘与回读校验。
//
// AS2 原版的编辑器和引擎都没有 SCRIPT::Save()，只有读取用的 LoadVar。
// 这里按 LoadVar 的逆过程写出文件，格式与 AS1 编辑器 LOGIC::Save() 相同：
//   u32 栈对象数 + 栈对象表 + i32 符号数 + {名字\0 + 20 字节 SCRIPTVAR} + 文本区 + u32 字节码长度 + 字节码

struct SCRIPT;

// 把编译完成的 SCRIPT 写成 Legacy LGD；成功返回 true
bool WriteLegacyLgd(const SCRIPT& script, const char* outputPath);

// 用引擎自己的 Load()/LoadVar() 把 lgd 读回来，与编译结果逐项比较；一致返回 true
bool VerifyLegacyLgd(const SCRIPT& compiled, const char* lgdPath);
