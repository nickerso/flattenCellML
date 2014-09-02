#include <sstream>

#include "compactorreport.hpp"

CompactorReport::CompactorReport()
{
}

void CompactorReport::setSourceModel(iface::cellml_api::Model* model)
{
    mSourceModel = model;
}

void CompactorReport::setCurrentSourceModelVariable(iface::cellml_api::CellMLVariable* variable)
{
    mCurrentSourceModelVariable = variable;
}

void CompactorReport::setVariableForCompaction(iface::cellml_api::CellMLVariable* variable,
                                               iface::cellml_api::CellMLVariable* sourceVariable)
{
    mVariableForCompaction.push_back(VariablePair(variable, sourceVariable));
}

void CompactorReport::setCompactedVariable(iface::cellml_api::CellMLVariable* variable)
{
    // we can resolve the current source variable to its compacted version
    VariablePair currentVariable = mVariableForCompaction.back();
    mVariableForCompaction.pop_back();
    mCompactedVariables[currentVariable.second].push_back(VariablePair(variable, currentVariable.first));
    if (mVariableForCompaction.size() != 0)
    {
        VariablePair previousVariable = mVariableForCompaction.back();
        mCompactedDependencies[previousVariable.second].push_back(currentVariable.second);
    }
    else
    {
        mCompactedDependencies[mCurrentSourceModelVariable].push_back(currentVariable.second);
    }
}

std::wstring CompactorReport::getReport() const
{
    std::wstringstream report;
    report << L"Model Compaction Report\n"
           << L"=======================\n\n";
    if (! mErrorMessage.empty()) report << L"Error message: " << mErrorMessage << L"\n\n";
    std::wstring indent = L"";
    if (mVariableForCompaction.size() > 0)
    {
        report << L"Some variables have not been compacted.\n"
               << L"Uncompacted variables are given below.\n\n";
        for (int i=0; i<mVariableForCompaction.size(); ++i)
        {
            ObjRef<iface::cellml_api::CellMLVariable> variable = mVariableForCompaction[i].first;
            ObjRef<iface::cellml_api::CellMLVariable> srcVariable = mVariableForCompaction[i].second;
            for (int j=0;j<i;++j) indent += L"\t";
            report << indent << L"Compaction requested for variable: " << variable->componentName() << L" / "
                   << variable->name() << L";\n" << indent << L"with the actual source variable being: "
                   << srcVariable->componentName() << L" / " << srcVariable->name() << L"\n";
        }
    }
    report.flush();
    return report.str();
}
