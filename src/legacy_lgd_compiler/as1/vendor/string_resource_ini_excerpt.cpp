// 摘自 AS1 地图编辑器 src/engine/resource_ini.cpp（第 41~121 行），代码未改动。
//
// 原文件主要是 INI / 资源相关实现，会牵出大量编辑器依赖；
// 编译器只用到其中这几个 STRING 成员函数，所以单独摘出来。

#include "runtime.h"

STRING::STRING(const char* str,int len)
{
    if (len) Copy(str,len); else m_buf=EMPTY;
}

void STRING::Read(STREAM* file)
{
    *this="";
    char chunk[256];
    int n=0;
    int value=0;
    for (;;) {
        if (n==255) {
            chunk[n]=0;
            *this+=chunk;
            n=0;
        }

        value=0;
        if (file->Read(&value,1u))
            value=0;
        if (value==10)
            value=0;
        if (value!=13)
            chunk[n++]=static_cast<char>(value);

        // Canonical MapEdit.exe 0x00457F0A compares the zero-extended
        // 32-bit read value (JG), not a signed char.  This is essential for
        // CP1251 resource strings: bytes 0x80..0xFF must remain positive.
        if (value<=0)
            break;
    }

    // Retail stores the terminating NUL byte in chunk before leaving the loop.
    *this+=chunk;
}

void STRING::Write(STREAM* file) { file->Write(m_buf,strlen(m_buf)+1u); }

void STRING::RemoveEndChars(const char* chars)
{
    char* p=m_buf+(int)strlen(m_buf)-1;
    while (p>=m_buf && strchr(chars,*p)) --p;
    p[1]=0;
}

int STRING::Int()
{
    if (m_buf[1]!='x') return atoi(m_buf);
    int value=0;
    sscanf(m_buf,"%i",&value);
    return value;
}
