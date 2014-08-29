
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>

#include "ModelCompactor.hpp"
#include "cellmlutils.hpp"

// XML Namespaces
#define MATHML_NS L"http://www.w3.org/1998/Math/MathML"
#define CELLML_1_0_NS L"http://www.cellml.org/cellml/1.0#"
#define CELLML_1_1_NS L"http://www.cellml.org/cellml/1.1#"

class ModelCompactor
{
private:
    ObjRef<iface::cellml_api::Model> mModelIn;
    ObjRef<iface::cellml_api::Model> mModelOut;
    CellmlUtils mCellml;
    // keeping track of source variables, should work?
    std::map<ObjRef<iface::cellml_api::CellMLVariable>, ObjRef<iface::cellml_api::CellMLVariable> > mSourceVariables;
    // to keep track of things we might want to report to the user?
    std::vector<std::wstring> mReport;

    std::wstring defineUnits(iface::cellml_api::Units* sourceUnits)
    {
        std::wstring unitsName = sourceUnits->name();
        return unitsName;
    }

    int mapLocalVariable(iface::cellml_api::CellMLVariable* sourceVariable,
                         iface::cellml_api::CellMLComponent* destinationComponent,
                         iface::cellml_api::CellMLComponent* compactedModel)
    {
        ObjRef<iface::cellml_api::CellMLVariable> variable =
                mCellml.createVariableWithMatchingUnits(destinationComponent, sourceVariable);
        // always defined in the compated model component
        variable->publicInterface(iface::cellml_api::INTERFACE_IN);
        // and connect it to the source variable
        ObjRef<iface::cellml_api::CellMLVariable> source = sourceVariable->sourceVariable();
        ObjRef<iface::cellml_api::CellMLVariable> compactedModelSourceVariable =
                defineCompactedSourceVariable(compactedModel, source);
        if (compactedModelSourceVariable == NULL)
        {
            std::wcerr << L"ERROR compacting source variable: " << source->componentName() << L" / "
                       << source->name() << std::endl;
            return -1;
        }
        mCellml.connectVariables(compactedModelSourceVariable, variable);
        return 0;
    }

    ObjRef<iface::cellml_api::CellMLVariable>
    defineCompactedSourceVariable(iface::cellml_api::CellMLComponent* compactedModel,
                                  iface::cellml_api::CellMLVariable* sourceVariable)
    {
        // hand over to the CellML utils...
        return mCellml.createCompactedVariable(compactedModel, sourceVariable, mSourceVariables);
    }

public:
    ObjRef<iface::cellml_api::Model> CompactModel(iface::cellml_api::Model* modelIn)
    {
        std::wstring modelName = modelIn->name();
        std::wcout << L"Compacting model " << modelName << L" to a single CellML 1.0 component."
                   << std::endl;
        // grab a clone of the source model before we do anything that might instantiate imports.
        mModelIn = QueryInterface(modelIn->clone(true));
        mModelIn->fullyInstantiateImports();

        // Create the output model
        mModelOut = mCellml.createModel();
        std::wstring cname = L"Compacted__" + modelName;
        mModelOut->name(cname);
        mModelOut->cmetaId(mModelIn->cmetaId());

        ObjRef<iface::cellml_api::CellMLComponent> compactedComponent =
                mCellml.createComponent(mModelOut, L"compactedModelComponent", L"CompactedModelComponent");

        ObjRef<iface::cellml_api::CellMLComponent> localComponent =
                mCellml.createComponent(mModelOut, L"sourceModelVariables", L"OriginalVariables");

        if (mCellml.setSourceModel(mModelIn) != 0)
        {
            std::wcerr << L"Doh!" << std::endl;
            return NULL;
        }

        ObjRef<iface::cellml_api::CellMLComponentSet> localComponents = mModelIn->localComponents();
        ObjRef<iface::cellml_api::CellMLComponentIterator> lci = localComponents->iterateComponents();
        std::wstring vname, tmpName;
        while (true)
        {
            ObjRef<iface::cellml_api::CellMLComponent> lc = lci->nextComponent();
            if (lc == NULL) break;
            cname = lc->name();
            std::wcout << L"Adding variables from component: " << cname << L"; to the new model."
                          << std::endl;
            ObjRef<iface::cellml_api::CellMLVariableSet> vs = lc->variables();
            ObjRef<iface::cellml_api::CellMLVariableIterator> vsi = vs->iterateVariables();
            while (true)
            {
                ObjRef<iface::cellml_api::CellMLVariable> v = vsi->nextVariable();
                if (v == NULL) break;
                vname = v->name();
                tmpName = mCellml.uniqueVariableName(cname, vname);
                std::wcout << L"\t" << vname << L" ==> " << tmpName << std::endl;
                if (mapLocalVariable(v, localComponent, compactedComponent) == 0)
                {
                    ObjRef<iface::cellml_api::CellMLVariable> source = v->sourceVariable();
                    std::wcout << L"\t\tmapped to source: " << source->name() << std::endl;
                }
                else
                {
                    std::wcerr << L"Error mapping local variable?" << std::endl;
                    return NULL;
                }
            }
        }
        // serialise the generated model to a string to catch any special annotations we might
        // have created.
        std::wstring modelString = mCellml.modelToString(mModelOut);
        // and then parse the model back to check its all good
        ObjRef<iface::cellml_api::Model> newModel = mCellml.createModelFromString(modelString);
        return newModel;
    }
};

ObjRef<iface::cellml_api::Model> compactModel(iface::cellml_api::Model* model)
{
    ObjRef<iface::cellml_api::Model> new_model;
    {
        ModelCompactor compactor;
        new_model = compactor.CompactModel(model);
    }
    return new_model;
}
