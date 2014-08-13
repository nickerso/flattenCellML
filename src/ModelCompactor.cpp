
#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>
#include <wchar.h>

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
    // to keep track of things we might want to report to the user?
    std::vector<std::wstring> mReport;

    std::wstring defineUnits(iface::cellml_api::Units* sourceUnits)
    {
        std::wstring unitsName = sourceUnits->name();
        return unitsName;
    }

    int mapLocalVariable(iface::cellml_api::CellMLComponent* sourceComponent,
                         iface::cellml_api::CellMLVariable* sourceVariable,
                         iface::cellml_api::CellMLComponent* destinationComponent)
    {
        std::wstring s = mCellml.uniqueVariableName(sourceComponent->name(), sourceVariable->name());
        ObjRef<iface::cellml_api::CellMLVariable> variable =
                mCellml.createVariable(destinationComponent, s, sourceVariable->cmetaId());
        ObjRef<iface::cellml_api::Units> units;
        try
        {
            units = sourceVariable->unitsElement();
            s = mCellml.defineUnits(mModelOut, units);
        }
        catch (...)
        {
            // we couldn't get a corresponding units element in the source model - could either be a
            // built-in unit or an error
            s = sourceVariable->unitsName();
            if (! mCellml.builtinUnits(s))
            {
                std::wcerr << L"ERROR: unable to find the source units: " << s << std::endl;
                return -1;
            }
        }
        variable->unitsName(s);
        // initial value -> always goes to an equation (v = 1; or v1 = v2)
        return 0;
    }

public:
    /**
     * The main interface to the model compactor.
     * @param modelIn The model to compact down to a single component.
     * @return The newly created model containing a single component defining the compacted model.
     */
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

        ObjRef<iface::cellml_api::CellMLComponent> localComponent =
                mCellml.createComponent(mModelOut, L"compactedModelComponent", L"CompactedModelComponent");

        ObjRef<iface::cellml_api::CellMLComponent> compactedComponent =
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
                if (mapLocalVariable(lc, v, localComponent) == 0)
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

        return mModelOut;
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
