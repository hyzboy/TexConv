#include "cmdline.h"

namespace
{
    wchar_t ascii_lower(wchar_t c)
    {
        return (c >= L'A' && c <= L'Z') ? c + 32 : c;
    }

    /// 大小写不敏感前缀比较:str 以 prefix 开头
    bool case_prefix(std::wstring str, const wchar_t *prefix)
    {
        while(*prefix)
        {
            if(str.empty())
                return false;

            if(ascii_lower(str.front()) != ascii_lower(*prefix))
                return false;

            str.erase(str.begin());
            ++prefix;
        }
        return true;
    }
}//namespace

CmdParse::CmdParse(const int _argc, const wchar_t *const *_argv)
{
    for(int i = 0; i < _argc; i++)
        args.push_back(_argv[i] ? _argv[i] : L"");
}

int CmdParse::Find(const wchar_t *flag) const
{
    for(size_t i = 0; i < args.size(); i++)
    {
        if(case_prefix(args[i], flag))
            return int(i);
    }

    return -1;
}

bool CmdParse::GetString(const wchar_t *flag, const wchar_t **value) const
{
    const int index = Find(flag);

    if(index == -1)
        return false;

    std::wstring rest = args[index].substr(wcslen(flag));

    if(rest.empty() && index + 1 < int(args.size()))
        rest = args[index + 1];         // 值在下一个参数

    // 去引号(对齐 hgl GetString:取引号内内容)
    if(!rest.empty() && rest.front() == L'"')
    {
        rest.erase(rest.begin());

        const size_t close = rest.find(L'"');

        if(close != std::wstring::npos)
            rest.resize(close);
    }

    // GetString 是 const,但返回的指针要求稳定:利用 mutable 拷贝区
    // (调用方在 CmdParse 存活期内使用,可安全覆盖)
    mutable_values.push_back(rest);
    *value = mutable_values.back().c_str();

    return true;
}
