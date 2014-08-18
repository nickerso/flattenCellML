#include <string>
#include <cstring>
#include <cstdlib>
#include <locale>
#include <codecvt>

std::string wstring2string(const std::wstring &str)
{
    if (str != L"")
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t> > utf8conv;
        return utf8conv.to_bytes(str);
    }
    return(std::string(""));
}

std::wstring string2wstring(const std::string& str)
{
    if (str != "")
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t> > utf8conv;
        return utf8conv.from_bytes(str);
    }
    return(std::wstring(L""));
}
