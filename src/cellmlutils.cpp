#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "cellmlutils.hpp"

#include <CellMLBootstrap.hpp>
#include <CUSESBootstrap.hpp>

/**
 * Return a modified version of the base name which should be unique.
 * @param base The base name to use in creating a unique name.
 * @return The unique name.
 */
static std::wstring uniqueName(const std::wstring& base)
{
    static int counter = 0x0;
    std::wostringstream oss;
    oss << std::setw(5) << std::setfill(L'0') << std::hex << ++counter;
    std::wstring un = base;
    un += L"_";
    un += oss.str();
    return un;
}

/**
 * Compare two canonical CellML units definitions from different models and return true if they match.
 * @param u1 The first units definition.
 * @param u2 The second units definition.
 * @return true of the two units definitions match.
 */
static bool unitsMatch(iface::cellml_services::CanonicalUnitRepresentation* u1,
                       iface::cellml_services::CanonicalUnitRepresentation* u2)
{
    if (u1->length() != u2->length()) return false;
    // looking at the CUSES compatibleWith source code, it seems safe to assume that ordering of the base
    // units will always be consistent ?
    for (uint32_t i = 0; i < u1->length(); ++i)
    {
        ObjRef<iface::cellml_services::BaseUnitInstance> bu1 = u1->fetchBaseUnit(i);
        ObjRef<iface::cellml_services::BaseUnitInstance> bu2 = u2->fetchBaseUnit(i);
        if (bu1->unit()->name() != bu2->unit()->name()) return false;
        if (bu1->prefix() != bu2->prefix()) return false;
        if (bu1->exponent() != bu2->exponent()) return false;
        if (bu1->offset() != bu2->offset()) return false;
    }
    return true;
}

/**
 * Look for an existing units in the given model which matches the definition of the sourceUnits (from a different
 * model).
 * @param model The model in which to search.
 * @param cuses The CUSES for this model.
 * @param sourceUnits The canonical units representation for the desired units in the source model.
 * @return The name of the units if a matching units definition exists in this model, empty string if
 * no match found.
 */
static std::wstring findMatchingUnits(iface::cellml_api::Model* model, iface::cellml_services::CUSES* cuses,
                                      iface::cellml_services::CanonicalUnitRepresentation* sourceUnits)
{
    std::wstring matchingUnitsName = L"";
    ObjRef<iface::cellml_api::UnitsSet> unitsSet = model->localUnits();
    ObjRef<iface::cellml_api::UnitsIterator> unitsIterator = unitsSet->iterateUnits();
    while (true)
    {
        ObjRef<iface::cellml_api::Units> units = unitsIterator->nextUnits();
        if (units == NULL) break;
        ObjRef<iface::cellml_services::CanonicalUnitRepresentation> cur =
                cuses->getUnitsByName(model, units->name());
        if (unitsMatch(cur, sourceUnits))
        {
            matchingUnitsName = units->name();
            break;
        }
    }
    return matchingUnitsName;
}

CellmlUtils::CellmlUtils()
{
    mBootstrap = CreateCellMLBootstrap();
    mCusesBootstrap = CreateCUSESBootstrap();
}

CellmlUtils::~CellmlUtils()
{
    // nothing to do
}

ObjRef<iface::cellml_api::Model> CellmlUtils::createModel()
{
    return mBootstrap->createModel(L"1.0");
}

ObjRef<iface::cellml_api::CellMLComponent>
CellmlUtils::createComponent(iface::cellml_api::Model *model, const std::wstring &name,
                             const std::wstring &cmetaId) const
{
    ObjRef<iface::cellml_api::CellMLComponent> component = model->createComponent();
    component->name(name);
    component->cmetaId(cmetaId);
    model->addElement(component);
    return component;
}

ObjRef<iface::cellml_api::CellMLVariable>
CellmlUtils::createVariable(iface::cellml_api::CellMLComponent *component, const std::wstring &name) const
{
    ObjRef<iface::cellml_api::CellMLVariable> variable = component->modelElement()->createCellMLVariable();
    variable->name(name);
    component->addElement(variable);
    return variable;
}

std::wstring CellmlUtils::defineUnits(iface::cellml_api::Model *model, iface::cellml_api::Units *sourceUnits) const
{
    std::wstring unitsName;
    // generate the canonical units representation for the source units
    ObjRef<iface::cellml_services::CanonicalUnitRepresentation>
            cur = mSourceCuses->getUnitsByName(sourceUnits->parentElement(), sourceUnits->name());
    // make sure we have an up-to-date CUSES for the destination model
    ObjRef<iface::cellml_services::CUSES> cuses = mCusesBootstrap->createCUSESForModel(model, true);
    // we always define units on the model, so don't need to look for units in components
    unitsName = findMatchingUnits(model, cuses, cur);
    if (unitsName.empty())
    {
        // need to create the units definition
        std::wstring newUnitsName = uniqueSetName(model->localUnits(), sourceUnits->name());
        std::wcout << L"\t\tCreating new units for: " << sourceUnits->name() << L"; as "
                   << newUnitsName << std::endl;
        unitsName = createUnitsFromCanonical(model, cur, newUnitsName);
    }
    else std::wcout << L"\t\tUnits: " << sourceUnits->name() << L"; already defined as: " << unitsName << std::endl;
    return unitsName;
}

int CellmlUtils::setSourceModel(iface::cellml_api::Model *model)
{
    mSourceModel = model;
    // since we compare units across models, we don't care about the strictness of comparisons...
    mSourceCuses = mCusesBootstrap->createCUSESForModel(mSourceModel, true);
    if (mSourceCuses->modelError() != L"")
    {
        std::wcerr << L"Error creating the CUSES for the source model: " << std::endl;
        std::wcerr << mSourceCuses->modelError() << std::endl;
        return -1;
    }
    return 0;
}

std::wstring CellmlUtils::uniqueSetName(iface::cellml_api::NamedCellMLElementSet *namedSet, const std::wstring &name) const
{
    std::wstring uname = name;
    ObjRef<iface::cellml_api::NamedCellMLElement> el = namedSet->get(uname);
    if (el) uname = uniqueName(uname);
    return uname;
}

bool CellmlUtils::builtinUnits(const std::wstring &name)
{
    bool match = false;
    ObjRef<iface::cellml_services::CanonicalUnitRepresentation> cur = mSourceCuses->getUnitsByName(mSourceModel, name);
    if (cur) match = true;
    return match;
}

std::wstring CellmlUtils::createUnitsFromCanonical(iface::cellml_api::Model *model,
                                                   iface::cellml_services::CanonicalUnitRepresentation *canonicalUnits,
                                                   const std::wstring &name) const
{
    std::wstring unitsName;
    ObjRef<iface::cellml_api::Units> units = model->createUnits();
    units->name(name);
    model->addElement(units);
    for (uint32_t i = 0; i < canonicalUnits->length(); ++i)
    {
        ObjRef<iface::cellml_services::BaseUnitInstance> bu = canonicalUnits->fetchBaseUnit(i);
        ObjRef<iface::cellml_api::Unit> unit = model->createUnit();
        unit->units(bu->unit()->name());
        unit->multiplier(bu->prefix());
        unit->offset(bu->offset());
        unit->exponent(bu->exponent());
        units->addElement(unit);
    }
    unitsName = units->name();
    return unitsName;
}