#pragma once
// AS2 资料片一代（AS2C / AS2R）的 Legacy LGD（变体 B）写盘与回读校验。
//
// 资料片编辑器自带 SCRIPT::Save()，但它只会写到源文件旁边。
// 这里按 Save() 的函数体逐行照抄，只把输出路径改成参数：
//   u32 栈对象数 + 栈对象表 + i32 符号数 + {名字\0 + 24 字节 SCRIPTVAR} + u32 字节码长度 + 字节码

struct SCRIPT;

// 把编译完成的 SCRIPT 写成 Legacy LGD（变体 B）；成功返回 true
bool WriteLegacyLgd(const SCRIPT& script, const char* outputPath);

// 用引擎自己的 Load()/LoadVar() 把 lgd 读回来，与编译结果逐项比较；一致返回 true
bool VerifyLegacyLgd(const SCRIPT& compiled, const char* lgdPath);
