#ifndef UTILS_HPP
#define UTILS_HPP

std::string wstring2string(const wchar_t* str);
std::wstring string2wstring(const char* str);
std::string getCellMLMetadataAsRDFXMLString(const char* mbrurl);

#endif // UTILS_HPP
