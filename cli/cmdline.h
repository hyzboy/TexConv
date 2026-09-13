#pragma once
// 轻量命令行参数解析(语义对齐 hgl::util::CmdParse,纯 CRT/STL 实现):
//   * Find/Contains:按"参数以 flag 开头(大小写不敏感)"前缀匹配
//   * GetString:flag 后剩余部分为值;若参数恰好等于 flag,则取下一个参数;
//     值支持引号包裹(去引号)
// GetString 返回的指针在 CmdParse 对象存活期内有效。

#include <string>
#include <vector>

class CmdParse
{
public:

    CmdParse(const int argc, const wchar_t *const *argv);

    int  Find(const wchar_t *flag) const;                       ///< 未找到返回 -1
    bool Contains(const wchar_t *flag) const { return Find(flag) != -1; }
    bool GetString(const wchar_t *flag, const wchar_t **value) const;

private:

    std::vector<std::wstring> args;
    mutable std::vector<std::wstring> mutable_values;   // GetString 返回值的稳定存储
};
