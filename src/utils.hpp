#ifndef UTILS_HPP
#define UTILS_HPP

std::string wstring2string(const std::wstring &str);
std::wstring string2wstring(const std::string& str);

std::wstring formatNumber(const int value);
std::wstring formatNumber(const uint32_t value);
std::wstring formatNumber(const double value);

#endif // UTILS_HPP
