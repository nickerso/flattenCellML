#ifndef UTILS_HPP
#define UTILS_HPP

std::string wstring2string(const std::wstring &str);
std::wstring string2wstring(const std::string& str);

std::wstring formatNumber(const int value);
std::wstring formatNumber(const uint32_t value);
std::wstring formatNumber(const double value);

/**
 * Replace all occurances of <original> in the <src> string with <replacement>.
 * @param src The source string.
 * @param original The character to be replaced.
 * @param replacement The replacement character.
 * @return A copy of <src> with all occurances of <original> replaced by <replacement>.
 */
std::wstring replaceAll(const std::wstring& src, wchar_t original, wchar_t replacement);

/**
 * Replace all occurances of <original> in the <src> string with <replacement>.
 * @param src The source string.
 * @param original The substring to be replaced.
 * @param replacement The replacement string.
 * @return A copy of <src> with all occurances of <original> replaced by <replacement>.
 */
std::wstring replaceAll(const std::wstring &src, const std::wstring& original,
                        const std::wstring& replacement);

/**
 * Remove all occurances of <original> in the given string.
 * @param src The source string.
 * @param original The character to remove from the given source string.
 * @return A copy of the source string with all occurances of <original> removed.
 */
std::wstring removeAll(const std::wstring& src, wchar_t original);

#endif // UTILS_HPP
