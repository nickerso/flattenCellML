
#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>
#include <wchar.h>
#include <set>
#include <map>
#include <vector>


// This header file provides C++ specific templates to make working with the API
// easier.
#include "cellml-api-cxx-support.hpp"

// This is the standard C++ interface for the core CellML API.
#include <IfaceCellML_APISPEC.hxx>
// CellML DOM implementation
#include <IfaceDOM_APISPEC.hxx>

// This is a C++ binding specific header that defines how to get the bootstrap
// object.
#include <CellMLBootstrap.hpp>

// For user annotations
#include <IfaceAnnoTools.hxx>
#include <AnnoToolsBootstrap.hpp>

// For finding relevant components
#include <IfaceCeVAS.hxx>
#include <CeVASBootstrap.hpp>

#include "ModelCompactor.hpp"

// XML Namespaces
#define MATHML_NS L"http://www.w3.org/1998/Math/MathML"
#define CELLML_1_0_NS L"http://www.cellml.org/cellml/1.0#"
#define CELLML_1_1_NS L"http://www.cellml.org/cellml/1.1#"

class ModelCompactor
{
private:
    ObjRef<iface::cellml_api::Model> mModelIn;
    ObjRef<iface::cellml_api::Model> mModelOut;

    std::wstring uniqueVariableName(const std::wstring& cname, const std::wstring& vname) const
    {
        std::wstring name = cname;
        name += L"_";
        name += vname;
        return name;
    }

    std::wstring defineUnits(iface::cellml_api::Units* sourceUnits)
    {
        std::wstring unitsName = sourceUnits->name();
        return unitsName;
    }

    int mapLocalVariable(iface::cellml_api::CellMLComponent* sourceComponent, iface::cellml_api::CellMLVariable* sourceVariable,
                         iface::cellml_api::CellMLComponent* destinationComponent)
    {
        ObjRef<iface::cellml_api::CellMLVariable> variable = mModelOut->createCellMLVariable();
        std::wstring s = uniqueVariableName(sourceComponent->name(), sourceVariable->name());
        variable->name(s);
        s = variable->cmetaId();
        if (!s.empty()) variable->cmetaId(s);
        destinationComponent->addElement(variable);
        ObjRef<iface::cellml_api::Units> units;
        try
        {
            units = sourceVariable->unitsElement();
            s = defineUnits(units);
        }
        catch (...)
        {
            // pretty dumb way to indicate that there is no corresponding units element (i.e., its a built in units)
            s = sourceVariable->unitsName();
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
        ObjRef<iface::cellml_api::CellMLBootstrap> cbs = CreateCellMLBootstrap();
        mModelOut = cbs->createModel(L"1.0");
        std::wstring cname = L"Compacted__" + modelName;
        mModelOut->name(cname);
        mModelOut->cmetaId(mModelIn->cmetaId());

        ObjRef<iface::cellml_api::CellMLComponent> localComponent = mModelOut->createComponent();
        localComponent->name(L"compactedModelComponent");
        localComponent->cmetaId(L"CompactedModelComponent");
        mModelOut->addElement(localComponent);

        ObjRef<iface::cellml_api::CellMLComponent> compactedComponent = mModelOut->createComponent();
        compactedComponent->name(L"sourceModelVariables");
        compactedComponent->cmetaId(L"OriginalVariables");
        mModelOut->addElement(compactedComponent);

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
                tmpName = uniqueVariableName(cname, vname);
                std::wcout << L"\t" << vname << L" ==> " << tmpName << std::endl;
                mapLocalVariable(lc, v, localComponent);
                ObjRef<iface::cellml_api::CellMLVariable> source = v->sourceVariable();
                std::wcout << L"\t\tmapped to source: " << source->name() << std::endl;
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
