#ifndef COMPACTORREPORT_HPP
#define COMPACTORREPORT_HPP

#include <map>
#include <vector>

#include <IfaceCellML_APISPEC.hxx>
#include <cellml-api-cxx-support.hpp>

typedef std::pair<ObjRef<iface::cellml_api::CellMLVariable>, ObjRef<iface::cellml_api::CellMLVariable> > VariablePair;
typedef std::vector<ObjRef<iface::cellml_api::CellMLVariable> > VariableVector;
typedef std::vector<VariablePair> VariablePairVector;
typedef std::map<ObjRef<iface::cellml_api::CellMLVariable>, VariablePair> VariablePairMap;
typedef std::map<ObjRef<iface::cellml_api::CellMLVariable>, VariableVector> VariableVectorMap;
typedef std::map<ObjRef<iface::cellml_api::CellMLVariable>, VariablePairVector> VariablePairVectorMap;

class CompactorReport
{
public:
    CompactorReport();

    void setSourceModel(iface::cellml_api::Model* model);
    void setCurrentSourceModelVariable(iface::cellml_api::CellMLVariable* variable);
    void setVariableForCompaction(iface::cellml_api::CellMLVariable* variable,
                                  iface::cellml_api::CellMLVariable* sourceVariable);
    void setCompactedVariable(iface::cellml_api::CellMLVariable* variable);

    void setErrorMessage(const std::wstring& msg)
    {
        mErrorMessage = msg;
    }

    std::wstring getReport() const;

private:
    ObjRef<iface::cellml_api::Model> mSourceModel;
    ObjRef<iface::cellml_api::CellMLVariable> mCurrentSourceModelVariable;
    VariablePairVector mVariableForCompaction; // first = variable requested, second = its source variable being compacted.
    VariablePairVectorMap mCompactedVariables;
    VariableVectorMap mCompactedDependencies;
    std::wstring mErrorMessage;
};

#endif // COMPACTORREPORT_HPP
