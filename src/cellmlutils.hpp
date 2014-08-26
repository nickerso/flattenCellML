#ifndef CELLMLUTILS_HPP
#define CELLMLUTILS_HPP

#include <map>

#include <cellml-api-cxx-support.hpp>
#include <IfaceCellML_APISPEC.hxx>
#include <IfaceCUSES.hxx>
#include <IfaceAnnoTools.hxx>

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

    /**
     * Create a new model from the given model definition string.
     * @param modelString A string containing the model definition.
     * @return The model created from the given string; or NULL on error.
     */
    ObjRef<iface::cellml_api::Model> createModelFromString(const std::wstring& modelString);

    ObjRef<iface::cellml_api::CellMLComponent>
    createComponent(iface::cellml_api::Model* model, const std::wstring& name,
                    const std::wstring& cmetaId) const;

    ObjRef<iface::cellml_api::CellMLVariable>
    createVariable(iface::cellml_api::CellMLComponent* component, const std::wstring& name) const;

    /**
     * Create a variable in the given component which has units which are equivalent to those of the source variable
     * in its model. The units for the variable will be defined in the given component's model if they do not already
     * exist.
     * @param component The component in which to create the variable.
     * @param sourceVariable The source variable which to duplicate in the given component. Expected to originate from
     * a different model.
     * @return The newly created variable on success; otherwise NULL.
     */
    ObjRef<iface::cellml_api::CellMLVariable>
    createVariableWithMatchingUnits(iface::cellml_api::CellMLComponent* component,
                                    iface::cellml_api::CellMLVariable* sourceVariable);

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

    /**
     * Ensure there exists a connection between the two variables. Will do nothing if connection already exists,
     * otherwise will create the connection.
     * @param v1 A variable to connect to v2.
     * @param v2 A variable to connect to v1.
     * @return zero on success.
     */
    int connectVariables(iface::cellml_api::CellMLVariable* v1, iface::cellml_api::CellMLVariable* v2);
    
    /**
     * Compact the given source variable as the specified variable. Will work out how the source variable is
     * defined and ensure all required variables are also defined in the variable's component, along with any
     * required math.
     * @param variable The copy of the source variable in the compacted model component.
     * @param sourceVariable The actual source variable from the original model.
     * @param compactedVariables The list of variables in the compacted model component to actual source
     * variables in the original model.
     * @return zero on success.
     */
    int compactVariable(iface::cellml_api::CellMLVariable* variable,
                        iface::cellml_api::CellMLVariable* sourceVariable,
                        std::map<ObjRef<iface::cellml_api::CellMLVariable>,
                                 ObjRef<iface::cellml_api::CellMLVariable> >& compactedVariables);

    /**
     * Generate a serialised version of the given model, with any annotations we know about
     * added in to the model serialisation.
     * @param model The model to serialise.
     * @return A string containing the serialised model. Will be an empty string if an error occurs.
     */
    std::wstring modelToString(iface::cellml_api::Model* model);

private:
    ObjRef<iface::cellml_api::CellMLBootstrap> mBootstrap;
    ObjRef<iface::cellml_api::Model> mSourceModel;
    ObjRef<iface::cellml_services::CUSESBootstrap> mCusesBootstrap;
    ObjRef<iface::cellml_services::CUSES> mSourceCuses;
    ObjRef<iface::cellml_services::AnnotationSet> mAnnotations;
    enum SourceVariableType
    {
        UNKNOWN = 0,
        DIFFERENTIAL = 1,
        ALGEBRACIC_LHS = 2,
        CONSTANT_PARAMETER_EQUATION = 3,
        CONSTANT_PARAMETER = 4,
        VARIABLE_OF_INTEGRATION = 5,
        SIMPLE_ASSIGNMENT = 6
    };
    static const std::wstring variableTypeToString(SourceVariableType vt)
    {
        switch (vt)
        {
        case DIFFERENTIAL:
            return L"Differential Variable";
        case ALGEBRACIC_LHS:
            return L"Algebraic LHS Variable";
        case CONSTANT_PARAMETER_EQUATION:
            return L"Constant Parameter (equation) Variable";
        case CONSTANT_PARAMETER:
            return L"Constant Parameter (initial_value) Variable";
        case VARIABLE_OF_INTEGRATION:
            return L"Variable of Integration";
        case SIMPLE_ASSIGNMENT:
            return L"Simple Assignment";
        default:
            return L"Unknown Variable Type";
        }
    }

    /**
     * Attempt to determine the type of the given source variable.
     * @param variable The variable of interest.
     * @param variableType The type of the variable, if it can be determined.
     * @return If the variable is of a type defined by MathML, a string containing the serialised MathML will be
     * returned. Otherwise an empty string is returned.
     */
    std::wstring determineSourceVariableType(iface::cellml_api::CellMLVariable* variable,
                                             SourceVariableType& variableType);

    /**
     * Attempt to get the initial_value for the given variable. Will trace back through the model if the initial_value
     * is set to be defined by a variable. Will also attempt to get the numerical value if the initial_value of the given
     * variable is set to a variable which has a simple numerical assignment.
     * @param variable The variable to get an initial_value from.
     * @param value The numerical value of the initial value.
     * @param level Used in the case where we recurse through the model.
     * @return 1 if an initial value was successfully found and value was set; zero on other success (i.e., no initial value attribute
     * found); other non-zero values if we are unable to determine the initial_value (e.g., if the linked variable
     * is defined by an algebraic expression).
     */
    int getInitialValue(iface::cellml_api::CellMLVariable* variable, double* value, int level);

    /**
     * Define the mathematics annotation for the given constant parameter equation.
     * @param component The component in which to create the equation.
     * @param vname The name of the constant parameter variable in the given component.
     * @param value The value to set in the equation.
     * @param unitsName The name of the units to give the numerical constant.
     * @return zero on success.
     */
    int defineConstantParameterEquation(iface::cellml_api::CellMLComponent* component,
                                        const std::wstring& vname, double value,
                                        const std::wstring& unitsName);
};

#endif // CELLMLUTILS_HPP
