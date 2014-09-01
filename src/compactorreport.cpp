#include "compactorreport.hpp"

CompactorReport::CompactorReport()
{
}

std::wstring CompactorReport::getIndentString() const
{
    return mIndentString;
}

void CompactorReport::setIndentString(const std::wstring& value)
{
    mIndentString = value;
}

int CompactorReport::getIndentLevel() const
{
    return mIndentLevel;
}

void CompactorReport::setIndentLevel(int value)
{
    mIndentLevel = value;
}

void CompactorReport::addReportLine(const std::wstring &line)
{
    int i;
    for (i=0; i<mIndentLevel; ++i) mReport << mIndentString;
    mReport << line << std::endl;
}
