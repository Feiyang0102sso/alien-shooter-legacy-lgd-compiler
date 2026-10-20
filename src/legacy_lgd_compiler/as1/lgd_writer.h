#pragma once
// as1（AS1 / Theseus）的 Legacy LGD（变体 A）写盘与回读校验。
//
// LOGIC 的栈、符号表、字节码都是 private 成员，外部函数访问不到，
// 所以写盘与比较分别由 vendor 头文件里新增的两个成员 LOGIC::SaveAs / LOGIC::SameAs 完成，
// 这里只是对它们的包装。

class LOGIC;

// 把编译完成的 LOGIC 写成 Legacy LGD；成功返回 true
bool WriteLegacyLgd(LOGIC& logic, const char* outputPath);

// 用引擎自己的 LOGIC::Load() 把 lgd 读回来，与编译结果逐项比较；一致返回 true
bool VerifyLegacyLgd(LOGIC& compiled, const char* lgdPath);
