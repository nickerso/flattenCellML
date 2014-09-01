#ifndef COMPACTORREPORT_HPP
#define COMPACTORREPORT_HPP

#include <string>
#include <sstream>

class CompactorReport
{
public:
    CompactorReport();

    std::wstring getReport() const
    {
        return mReport.str();
    }

    std::wstring getIndentString() const;
    void setIndentString(const std::wstring& value);

    int getIndentLevel() const;
    void setIndentLevel(int value);

    void addReportLine(const std::wstring& line);

    void incrementIndentLevel()
    {
        ++mIndentLevel;
    }

    void decrementIndentLevel()
    {
        --mIndentLevel;
    }

private:
    std::wstringstream mReport;
    std::wstring mIndentString;
    int mIndentLevel;
};

#endif // COMPACTORREPORT_HPP
