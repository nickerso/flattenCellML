
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
        mModelIn = modelIn;

        // Create the output model
        ObjRef<iface::cellml_api::CellMLBootstrap> cbs = CreateCellMLBootstrap();
        mModelOut = cbs->createModel(L"1.0");
        std::wstring cname = L"Compacted__" + modelName;
        mModelOut->name(cname);
        mModelOut->cmetaId(mModelIn->cmetaId());

        ObjRef<iface::cellml_api::CellMLComponent> component = mModelOut->createComponent();
        component->name(L"model");
        component->cmetaId(L"CompactedModelComponent");
        mModelOut->addElement(component);

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
