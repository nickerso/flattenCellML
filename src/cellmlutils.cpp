#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "cellmlutils.hpp"
#include "xmlutils.hpp"
#include "utils.hpp"

#include <CellMLBootstrap.hpp>
#include <CUSESBootstrap.hpp>
#include <AnnoToolsBootstrap.hpp>

#define MATH_ANNOTATION_KEY L"mathml::math"

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

static ObjRef<iface::cellml_api::Connection>
findConnection(ObjRef<iface::cellml_api::ConnectionSet> connections, const std::wstring& c1,
               const std::wstring& c2, int* order)
{
    ObjRef<iface::cellml_api::Connection> connection;
    ObjRef<iface::cellml_api::ConnectionIterator> ci = connections->iterateConnections();
    while (true)
    {
        connection = ci->nextConnection();
        if (connection == NULL) break;
        ObjRef<iface::cellml_api::MapComponents> cmap = connection->componentMapping();
        const std::wstring& cmap1 = cmap->firstComponentName();
        const std::wstring& cmap2 = cmap->secondComponentName();
        if ((cmap1 == c1) && (cmap2 == c2))
        {
            *order = 1;
            break;
        }
        else if ((cmap1 == c2) && (cmap2 == c1))
        {
            *order = 2;
            break;
        }
    }
    return connection;
}

static ObjRef<iface::cellml_api::Connection>
createConnection(iface::cellml_api::Model* model, const std::wstring& c1, const std::wstring& c2)
{
    ObjRef<iface::cellml_api::Connection> connection = model->createConnection();
    model->addElement(connection);
    ObjRef<iface::cellml_api::MapComponents> cmap = connection->componentMapping();
    cmap->firstComponentName(c1);
    cmap->secondComponentName(c2);
    return connection;
}

static void
defineMapVariables(iface::cellml_api::Model* model, iface::cellml_api::Connection* connection,
                   const std::wstring& v1, const std::wstring& v2)
{
    ObjRef<iface::cellml_api::MapVariablesSet> mvs = connection->variableMappings();
    bool found = false;
    ObjRef<iface::cellml_api::MapVariablesIterator> mvi = mvs->iterateMapVariables();
    while (true)
    {
        ObjRef<iface::cellml_api::MapVariables> vmap = mvi->nextMapVariable();
        if (vmap == NULL) break;
        if ((vmap->firstVariableName() == v1) && (vmap->secondVariableName() == v2))
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        ObjRef<iface::cellml_api::MapVariables> vmap = model->createMapVariables();
        connection->addElement(vmap);
        vmap->firstVariableName(v1);
        vmap->secondVariableName(v2);
    }
}

int CellmlUtils::getInitialValue(iface::cellml_api::CellMLVariable* variable, double* value, int level)
{
    int returnCode = 0;
    if (variable->initialValue() != L"")
    {
        // an initial value is present
        if (variable->initialValueFromVariable())
        {
            ObjRef<iface::cellml_api::CellMLVariable> ivSource =
                    variable->initialValueVariable()->sourceVariable();
            return getInitialValue(ivSource, value, ++level);
        }
        else
        {
            /// @todo Need to ensure unit conversion happens.
            // numerical initial_value
            *value = variable->initialValueValue();
            return 1;
        }
    }
    else if (level > 0)
    {
        // we have a variable used as the initial_value on another variable, but it does not have an
        // initial_value attribute - so it is probably defined in an equation. Check for the easy case
        // we can handle
        SourceVariableType vt;
        std::wstring mathml = determineSourceVariableType(variable, vt);
        if (vt == CONSTANT_PARAMETER_EQUATION)
        {
            std::wcout << L"getInitialValue: Found a constant parameter equation for "
                       << variable->componentName() << L"/" << variable->name() << std::endl;
            XmlUtils xutils;
            xutils.parseString(mathml);
            std::wstring unitsName;
            returnCode = xutils.numericalAssignmentGetValue(value, unitsName);
            /// @todo Need to match units.
            std::wcout << L"Getting value for: " << variable->componentName() << L"/"
                       << variable->name() << std::endl;
            std::wcout << L"units = \"" << unitsName << L"\"" << std::endl;
            return 1;
        }
        else
        {
            std::wcerr << L"ERROR: initial value set by variable (" << variable->componentName() << L"/"
                       << variable->name() << L"which isn't defined in a way we can use for a initial_value."
                       << std::endl;
            return -1;
        }
    }
    return returnCode;
}

CellmlUtils::CellmlUtils()
{
    mBootstrap = CreateCellMLBootstrap();
    mCusesBootstrap = CreateCUSESBootstrap();
    ObjRef<iface::cellml_services::AnnotationToolService> ats =  CreateAnnotationToolService();
    // create one annotation set to use for our annotations.
    mAnnotations = ats->createAnnotationSet();
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

ObjRef<iface::cellml_api::CellMLVariable>
CellmlUtils::createVariableWithMatchingUnits(iface::cellml_api::CellMLComponent* component,
                                             iface::cellml_api::CellMLVariable* sourceVariable)
{
    std::wstring s = uniqueVariableName(sourceVariable->componentName(), sourceVariable->name());
    s = uniqueSetName(component->variables(), s);
    ObjRef<iface::cellml_api::CellMLVariable> variable = createVariable(component, s);
    try
    {
        ObjRef<iface::cellml_api::Units> units = sourceVariable->unitsElement();
        s = defineUnits(component->modelElement(), units);
    }
    catch (...)
    {
        // we couldn't get a corresponding units element in the source model - could either be a
        // built-in unit or an error
        s = sourceVariable->unitsName();
        if (! builtinUnits(s))
        {
            std::wcerr << L"ERROR: unable to find the source units: " << s << std::endl;
            return NULL;
        }
    }
    variable->unitsName(s);
    return variable;
}

int CellmlUtils::connectVariables(iface::cellml_api::CellMLVariable *v1, iface::cellml_api::CellMLVariable *v2)
{
    int returnCode = 0;
    try
    {
        int order = 0;
        ObjRef<iface::cellml_api::Connection> connection = findConnection(v1->modelElement()->connections(),
                                                                          v1->componentName(),
                                                                          v2->componentName(), &order);
        if (connection == NULL)
        {
            connection = createConnection(v1->modelElement(), v1->componentName(), v2->componentName());
            order = 1;
        }
        if (order == 1) defineMapVariables(v1->modelElement(), connection, v1->name(), v2->name());
        else defineMapVariables(v1->modelElement(), connection, v2->name(), v1->name());
    }
    catch (...)
    {
        std::wcerr << L"CellmlUtils::connectVariables: Error caught trying to define connection:"
                   << v1->componentName() << L"/" << v1->name() << L" <==> "
                   << v2->componentName() << L"/" << v2->name() << std::endl;
        return -1;
    }
    return returnCode;
}

int CellmlUtils::compactVariable(iface::cellml_api::CellMLVariable *variable,
                                 iface::cellml_api::CellMLVariable *sourceVariable,
                                 std::map<ObjRef<iface::cellml_api::CellMLVariable>,
                                          ObjRef<iface::cellml_api::CellMLVariable> >& compactedVariables)
{
    int returnCode = 0;
    // add the variable to the compacted variable list so that we don't try to work on in multiple times
    // need to be sure to remove it if any error occurs.
    compactedVariables[sourceVariable] = variable;

    // determine what sort of source variable we are dealing with
    SourceVariableType vt;
    std::wstring mathml = determineSourceVariableType(sourceVariable, vt);
    if (! mathml.empty())
    {
        std::wcout << L"Source variable: " << sourceVariable->componentName() << L" / " << sourceVariable->name()
                   << L"; is of type: " << variableTypeToString(vt) << std::endl;
        switch (vt)
        {
        case DIFFERENTIAL:
        case ALGEBRACIC_LHS:
            break;
        case CONSTANT_PARAMETER_EQUATION:
        {
            XmlUtils xutils;
            xutils.parseString(mathml);
            /// @todo Need to make sure units are defined?
            std::wstring unitsName;
            double value;
            returnCode = xutils.numericalAssignmentGetValue(&value, unitsName);
            ObjRef<iface::cellml_api::CellMLComponent> component(QueryInterface(variable->parentElement()));
            returnCode = defineConstantParameterEquation(component, variable->name(), value,
                                            unitsName);
        } break;
        case CONSTANT_PARAMETER:
        case VARIABLE_OF_INTEGRATION:
        case SIMPLE_EQUALITY:
        default:
            break;
        }
    }

    if (returnCode != 0)
    {
        std::wcerr << L"ERROR: CellmlUtils::compactVariable: Something went wrong compacting the source variable: "
                   << sourceVariable->componentName() << L" / " << sourceVariable->name() << std::endl;
        // unsuccessfully compacted, so remove it to the list of compacted source variables.
        compactedVariables.erase(sourceVariable);
        return returnCode;
    }

    // handle the initial value attribute
    double iv;
    returnCode = getInitialValue(sourceVariable, &iv, 0);
    if (returnCode == 1) variable->initialValueValue(iv);
    else if (returnCode != 0)
    {
        std::wcerr << L"Unable to handle the case of initial value's which are not resolvable "
                      L"to a specified value "
                   << sourceVariable->componentName() << L" / " << sourceVariable->name() << std::endl;
        // unsuccessfully compacted, so remove it to the list of compacted source variables.
        compactedVariables.erase(sourceVariable);
        return -1;
    }
    return 0;
}

std::wstring CellmlUtils::determineSourceVariableType(iface::cellml_api::CellMLVariable *variable,
                                                      CellmlUtils::SourceVariableType& variableType)
{
    std::wstring mathString = L"";
    variableType = UNKNOWN;
    ObjRef<iface::cellml_api::CellMLComponent> component = QueryInterface(variable->parentElement());
    ObjRef<iface::cellml_api::MathList> mathList = component->math();
    ObjRef<iface::cellml_api::MathMLElementIterator> iter = mathList->iterate();
    XmlUtils xmlUtils;
    while (true)
    {
        ObjRef<iface::mathml_dom::MathMLElement> mathElement = iter->next();
        if (mathElement == NULL) break;
        // make sure its a mathml:math element?
        ObjRef<iface::mathml_dom::MathMLMathElement> math = QueryInterface(mathElement);
        if (math)
        {
            std::wstring str = mBootstrap->serialiseNode(math);
            // str = L"<?xml version=\"1.0\"?>\n" + str;
            // std::wcout << L"Math block: " << str << std::endl;
            xmlUtils.parseString(str);
            mathString = xmlUtils.matchConstantParameterEquation(variable->name());
            if (! mathString.empty())
            {
                //std::wcout << L"Math is a constant parameter equation: **" << mathString << L"**" << std::endl;
                variableType = CONSTANT_PARAMETER_EQUATION;
                break;
            }
            mathString = xmlUtils.matchSimpleEquality(variable->name());
            if (! mathString.empty())
            {
                std::wcout << L"Math is a simple equality: **" << mathString << L"**" << std::endl;
                variableType = SIMPLE_EQUALITY;
                break;
            }
            mathString = xmlUtils.matchAlgebraicLhs(variable->name());
            if (! mathString.empty())
            {
                variableType = ALGEBRACIC_LHS;
                break;
            }
        }
    }
    return mathString;
}

int CellmlUtils::defineConstantParameterEquation(iface::cellml_api::CellMLComponent* component,
                                                 const std::wstring& vname, double value,
                                                 const std::wstring& unitsName)
{
    int returnCode = 0;
    std::wstring mathml = L"<apply><eq/><ci>";
    mathml += vname;
    mathml += L"</ci><cn cellml:units=\"";
    mathml += unitsName;
    mathml += L"\">";
    mathml += formatNumber(value);
    mathml += L"</cn></apply>";
    // make sure we add in any existing annotations...
    mathml += mAnnotations->getStringAnnotation(component, MATH_ANNOTATION_KEY);
    mAnnotations->setStringAnnotation(component, MATH_ANNOTATION_KEY, mathml);
    return returnCode;
}

std::wstring CellmlUtils::modelToString(iface::cellml_api::Model *model)
{
    std::wstring modelString = model->serialisedText();
    // dirty hack to get the math into the model since its too hard to do this directly in the CellML API.
    ObjRef<iface::cellml_api::CellMLComponentSet> components = model->localComponents();
    ObjRef<iface::cellml_api::CellMLComponentIterator> iter = components->iterateComponents();
    while (true)
    {
        ObjRef<iface::cellml_api::CellMLComponent> component = iter->nextComponent();
        if (component == NULL) break;
        std::wstring cname = component->name();
        std::wstring mathBlock = mAnnotations->getStringAnnotation(component, MATH_ANNOTATION_KEY);
        if (mathBlock.length() > 1)
        {
            mathBlock = L"<math xmlns=\"http://www.w3.org/1998/Math/MathML\">" + mathBlock;
            mathBlock += L"</math>";
            //std::wcout << L"Adding math block to component: " << mathBlock << std::endl;
            std::wstring nameAttr = L"name=\"" + cname + L"\"";
            size_t componentNameLocation = modelString.find(nameAttr);
            size_t componentEndLocation = modelString.find(L"</component>", componentNameLocation);
            modelString.insert(componentEndLocation, mathBlock);
        }
    }
    // seems we need to make sure the cellml namespace prefix is defined
    if (modelString.find(L"xmlns:cellml") == modelString.npos)
    {
        std::size_t pos1 = modelString.find(L"xmlns=\"");
        pos1 += 7;
        std::size_t pos2 = modelString.find(L'"', pos1);
        std::wstring nsUri = modelString.substr(pos1, (pos2-pos1));
        modelString.insert(++pos2, L" xmlns:cellml=\"");
        pos2 += 15;
        modelString.insert(pos2, nsUri);
        pos2 += nsUri.length();
        modelString.insert(pos2, L"\"");
    }
    return modelString;
}

ObjRef<iface::cellml_api::Model> CellmlUtils::createModelFromString(const std::wstring &modelString)
{
    ObjRef<iface::cellml_api::Model> newModel = NULL;
    ObjRef<iface::cellml_api::DOMModelLoader> modelLoader = mBootstrap->modelLoader();
    try
    {
        newModel = modelLoader->createFromText(modelString);
    }
    catch (...)
    {
        std::wcerr << L"ERROR creating model from the string: " << modelString << std::endl;
    }
    return newModel;
}
