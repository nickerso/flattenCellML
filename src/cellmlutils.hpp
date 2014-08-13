#ifndef CELLMLUTILS_HPP
#define CELLMLUTILS_HPP

#include "cellml-api-cxx-support.hpp"
#include <IfaceCellML_APISPEC.hxx>
#include <IfaceCUSES.hxx>

class CellmlUtils
{
public:
    CellmlUtils();
    ~CellmlUtils();

    /**
     * Create a CellML 1.0 model.
     * @return The newly created model.
     */
    ObjRef<iface::cellml_api::Model> createModel();

    ObjRef<iface::cellml_api::CellMLComponent>
    createComponent(iface::cellml_api::Model* model, const std::wstring& name,
                    const std::wstring& cmetaId) const;

    ObjRef<iface::cellml_api::CellMLVariable>
    createVariable(iface::cellml_api::CellMLComponent* component, const std::wstring& name,
                   const std::wstring& cmetaId) const;

    /**
     * Converts the given source units to their cononical representation and ensures they are defined
     * in the given model.
     * @param model The model in which to ensure units is defined.
     * @param sourceUnits The units to "copy" into the given model.
     * @return The name of the units created in the given model (may or may not be the same as the source units).
     */
    std::wstring defineUnits(iface::cellml_api::Model* model, iface::cellml_api::Units* sourceUnits) const;

    /**
     * Create a new units in the given model with the provided name, defined by the specified canonical
     * units representation. The canonical units are expected to originate from a different source model
     * so the units can not be simply copied in.
     * @param model The model in which to create the units.
     * @param canonicalUnits The canonical units representation from the source model.
     * @param name The name to give the newly created units.
     * @return The name of the newly created units on success; the empty string on failure.
     */
    std::wstring createUnitsFromCanonical(iface::cellml_api::Model* model,
                                          iface::cellml_services::CanonicalUnitRepresentation* canonicalUnits,
                                          const std::wstring& name) const;

    /**
     * Grab hold of the source model, will trigger the building of any extra stuff we might need from the model.
     * @param model The model that will be the source for model compaction.
     * @return zero on success.
     */
    int setSourceModel(iface::cellml_api::Model* model);

    std::wstring uniqueVariableName(const std::wstring& cname, const std::wstring& vname) const
    {
        std::wstring name = cname;
        name += L"_";
        name += vname;
        return name;
    }

    /**
     * Create a unique name within the given named element set based on the specified baseName.
     * @param namedSet The named element set in which to ensure a unique name.
     * @param name The preferred name.
     * @return A unique name for use in the given named element set. Will return <name> if it is unique.
     */
    std::wstring uniqueSetName(iface::cellml_api::NamedCellMLElementSet* namedSet, const std::wstring& name) const;

    /**
     * Determine if the given units name is a valid "built-in" units name in CellML.
     * @param name The units name to check.
     * @return true if the units name is a built-in units; false otherwise.
     */
    bool builtinUnits(const std::wstring& name);

private:
    ObjRef<iface::cellml_api::CellMLBootstrap> mBootstrap;
    ObjRef<iface::cellml_api::Model> mSourceModel;
    ObjRef<iface::cellml_services::CUSESBootstrap> mCusesBootstrap;
    ObjRef<iface::cellml_services::CUSES> mSourceCuses;
};

#endif // CELLMLUTILS_HPP
