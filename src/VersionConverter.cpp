/**
 * Based on:
 *   Test program using the CellML API.
 *
 *   from Jonathan Cooper
 *
 * First attempt at a CellML 1.1 -> 1.0 converter.
 *
 * Cannot handle:
 *  * initial_value="variable_name"
 *    (At least to maintain the same semantics.  If the referenced variable
 *     has a numeric initial_value, then we do use that directly.)
 *
 * Does not handle:
 *  * reaction elements
 *  * different units definitions with the same name
 *    (in fact the units copying is rather dodgy, but likely to work OK in most
 *     cases)
 *  * extension attributes on CellML elements (due to lack of API support)
 *  * re-building the containment hierarchy (since contained components
 *    aren't necessarily needed for simulation)
 *  * copying RDF metadata reliably (unless it gets copied by virtue of being
 *    an extension element)
 *  * no pretty-printing of the output
 *
 */

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

// Save typing
namespace cml = iface::cellml_api;
namespace cmlsvs = iface::cellml_services;
namespace dom = iface::dom;

// XML Namespaces
#define MATHML_NS L"http://www.w3.org/1998/Math/MathML"
#define CELLML_1_0_NS L"http://www.cellml.org/cellml/1.0#"
#define CELLML_1_1_NS L"http://www.cellml.org/cellml/1.1#"

/**
 * Utility looping macros.
 * These macros are designed to be used like:
 *
 * ITERATE2(var, Variable, iface::cellml_api::CellMLVariable, component->variables())
 * {
 *     // do stuff with var
 * }
 *
 * RETURN_INTO_OBJREF(children, iface::cellml_api::CellMLComponentSet, root->encapsulationChildren());
 * if (children->length() > 0)
 * {
 *     ITERATE(child, Component, iface::cellml_api::CellMLComponent, children)
 *     {
 *     }
 * }
 *
 * The ITERATE_S variant can be used where a plural isn't formed by adding 's', e.g.
 * ITERATE_S(units, Units, Units, iface::cellml_api::Units, unitset)
 *
 * There are also 2 macros for cases which don't follow the typical naming pattern:
 *  ITERATE_CHILDREN(parent) - iterates over child CellMLElements
 *  ITERATE_MATH(thing, listexpr) - iterates over MathMLElements
 *
 * Note that for the macro variants which create set and iterator objects, the names
 * for these are based on 'thing' and they are placed in the calling scope, so beware
 * if you iterate the same thing multiple times in the same scope.  It may be better
 * to use a nested scope in future versions of these macros.
 */
#define ITERATE_BASE(thing, Type, iterator, next) \
    for (ObjRef<Type> thing = already_AddRefd<Type>(iterator->next()); \
    thing != NULL; \
    thing = already_AddRefd<Type>(iterator->next()))

#define ITERATE_ITER(thing, Type, IterType, iterate, next) \
    RETURN_INTO_OBJREF(iter_##thing, IterType, iterate()); \
    ITERATE_BASE(thing, Type, iter_##thing, next)

#define ITERATE_CHILDREN(parent)					\
    RETURN_INTO_OBJREF(children, iface::cellml_api::CellMLElementSet,	\
    parent->childElements());			\
    ITERATE_ITER(child, iface::cellml_api::CellMLElement,		\
    iface::cellml_api::CellMLElementIterator,		\
    children->iterate, next)

#define ITERATE_MATH(thing, listexpr)					\
    RETURN_INTO_OBJREF(list_##thing, cml::MathList, listexpr);		\
    ITERATE_ITER(thing, iface::mathml_dom::MathMLElement,		\
    cml::MathMLElementIterator, list_##thing->iterate, next)

#define ITERATE_S(thing, Thing, Plural, Type, set)			\
    RETURN_INTO_OBJREF(iter_##thing, Type##Iterator, set->iterate##Plural()); \
    ITERATE_BASE(thing, Type, iter_##thing, next##Thing)

#define ITERATE(thing, Thing, Type, set) \
    ITERATE_S(thing, Thing, Thing##s, Type, set)

#define ITERATE2(thing, Thing, Type, setexpr)		 \
    RETURN_INTO_OBJREF(set_##thing, Type##Set, setexpr);  \
    ITERATE(thing, Thing, Type, set_##thing)

/// Utility macro for copying attributes
#define COPY_ATTR(to, from) \
{ \
    std::wstring tmp = from(); \
    to(tmp); \
}

template<typename T>
bool set_contains(const std::set<T> s, const T& m)
{
    typename std::set<T>::const_iterator i = s.find(m);
    return (i != s.end());
}


/**
 * The workhorse.
 */
class VersionConverter
{
private:
    /// The model we're converting
    cml::Model* mModelIn;

    /// The model we're creating
    cml::Model* mModelOut;

    /// Manager for annotations on models
    iface::cellml_services::AnnotationSet* mAnnoSet;

    /// Units we have already copied into the new model
    std::set<std::pair<std::wstring,std::wstring> > mCopiedUnits;

    /// Component names used in the new model, to avoid duplicates
    std::set<std::wstring> mCompNames;

public:
    /** Reset member data to convert a new model. */
    void Reset()
    {
        mModelIn = NULL;
        mModelOut = NULL;
        mAnnoSet = NULL;
        mCopiedUnits.clear();
        mCompNames.clear();
    }

    /**
     * A utility method which employs CeVAS's algorithm to find the
     * 'real' component for the given component.  That is, if the
     * component is an ImportComponent, the template CellMLComponent
     * that it is based on in an imported model.  Otherwise returns
     * the given component.
     *
     * Requires imports to have been fully instantiated.  Returns NULL
     * if the real component does not exist.
     *
     * Also requires that a component is not imported more than once
     * in the same import element, as then multiple components share
     * one real component.
     */
    cml::CellMLComponent* FindRealComponent(cml::CellMLComponent* component)
    {
        component->add_ref();
        while (true)
        {
            DECLARE_QUERY_INTERFACE_OBJREF(impc, component,
                                           cellml_api::ImportComponent);
            if (impc == NULL)
                break; // Found a non-ImportComponent
            RETURN_INTO_OBJREF(imp_elt, cml::CellMLElement, impc->parentElement());
            DECLARE_QUERY_INTERFACE_OBJREF(imp, imp_elt, cellml_api::CellMLImport);
            RETURN_INTO_OBJREF(model, cml::Model, imp->importedModel());
            assert(model != NULL);
            RETURN_INTO_OBJREF(comps, cml::CellMLComponentSet,
                               model->modelComponents());
            RETURN_INTO_WSTRING(ref, impc->componentRef());
            component->release_ref();
            component = comps->getComponent(ref.c_str());
            if (component == NULL)
                break; // Real component does not exist
        }
        return component;
    }

    /**
     * Utility method to print out info about a connection.
     */
    void DisplayConnection(cml::Connection* conn)
    {
        RETURN_INTO_OBJREF(mc, cml::MapComponents, conn->componentMapping());
        RETURN_INTO_OBJREF(c1, cml::CellMLComponent, mc->firstComponent());
        RETURN_INTO_OBJREF(m1, cml::Model, c1->modelElement());
        RETURN_INTO_OBJREF(c2, cml::CellMLComponent, mc->secondComponent());
        RETURN_INTO_OBJREF(m2, cml::Model, c2->modelElement());

        RETURN_INTO_WSTRING(mname1, m1->name());
        RETURN_INTO_WSTRING(cname1, mc->firstComponentName());
        RETURN_INTO_WSTRING(mname2, m2->name());
        RETURN_INTO_WSTRING(cname2, mc->secondComponentName());

        std::wcout << "Connection: " << mname1 << "/" << cname1 << " <-> "
                   << mname2 << "/" << cname2 << " :" << std::endl;

        RETURN_INTO_OBJREF(varmaps, cml::MapVariablesSet,
                           conn->variableMappings());
        ITERATE(varmap, MapVariable, cml::MapVariables, varmaps)
        {
            RETURN_INTO_WSTRING(v1, varmap->firstVariableName());
            RETURN_INTO_WSTRING(v2, varmap->secondVariableName());
            std::wcout << "  Var: " << v1 << " <-> " << v2 << std::endl;
        }
    }

private:
    /**
     * Annotate all imported components (whether direct or indirect)
     * with the name they are given by the importing model.
     */
    void StoreImportRenamings(cml::Model* importingModel)
    {
        ITERATE2(import, Import, cml::CellMLImport, importingModel->imports())
        {
            assert(import->wasInstantiated());
            RETURN_INTO_OBJREF(imported_model, cml::Model,
                               import->importedModel());
            // Recursively process components imported by the imported
            // model.  Depth first processing means components renamed
            // twice end up with the right name.
            StoreImportRenamings(imported_model);

            ITERATE2(comp, ImportComponent, cml::ImportComponent,
                     import->components())
            {
                // Now get the component object to annotate, using the
                // same algorithm as CeVAS.
                RETURN_INTO_OBJREF(real_comp, cml::CellMLComponent,
                                   FindRealComponent(comp));

                RETURN_INTO_WSTRING(local_name, comp->name());
                mAnnoSet->setStringAnnotation(real_comp, L"renamed",
                                              local_name.c_str());
            }
        }
    }

    /**
     * Ensure that component names in the generated model are unique.
     *
     * If the given name has already been used, it is modified so as
     * to be unique, by appending the string "_n" where n is the least
     * natural number giving an unused name.
     */
    void EnsureComponentNameUnique(std::wstring& cname)
    {
        std::wstring suffix = L"";
        unsigned n = 0;
        while (mCompNames.find(cname + suffix) != mCompNames.end())
        {
            // Current attempt already used; try the next
            std::wstringstream _suffix;
            _suffix << L"_" << (++n);
            _suffix >> suffix;
        }
        cname += suffix;
        mCompNames.insert(cname);
    }

    /**
     * Copy any relevant connections into the new model.
     *
     * Copies across any connections defined in the given model
     * between 2 components which have been copied, and recursively
     * processes imported models.
     */
    void CopyConnections(cml::Model* model)
    {
        // Copy local connections
        ITERATE2(conn, Connection, cml::Connection, model->connections())
        {
            CopyConnection(conn);
        }
        // Process imported models
        ITERATE2(import, Import, cml::CellMLImport, model->imports())
        {
            RETURN_INTO_OBJREF(imp_model, cml::Model, import->importedModel());
            assert(imp_model != NULL);
            CopyConnections(imp_model);
        }
    }

    /**
     * Copy a connection, possibly involving imported components, into
     * our new model.
     *
     * The connection will only be copied if both components involved
     * have been copied across first, and hence have a "copy"
     * annotation.
     */
    void CopyConnection(cml::Connection* conn)
    {
        RETURN_INTO_OBJREF(mc, cml::MapComponents, conn->componentMapping());
        RETURN_INTO_OBJREF(c1, cml::CellMLComponent, mc->firstComponent());
        RETURN_INTO_OBJREF(c2, cml::CellMLComponent, mc->secondComponent());

        // Check we've copied the components involved, and get the
        // copies.
        ObjRef<cml::CellMLComponent> newc1 = QueryInterface(mAnnoSet->getObjectAnnotation(c1, L"copy"));
        if (newc1 == NULL)
            return;
        ObjRef<cml::CellMLComponent> newc2 = QueryInterface(mAnnoSet->getObjectAnnotation(c2, L"copy"));
        if (newc2 == NULL)
            return;

        // Create a new connection
        RETURN_INTO_OBJREF(newconn, cml::Connection,
                           mModelOut->createConnection());
        mModelOut->addElement(newconn);
        RETURN_INTO_OBJREF(newmc, cml::MapComponents,
                           newconn->componentMapping());
        COPY_ATTR(newmc->firstComponentName, newc1->name);
        COPY_ATTR(newmc->secondComponentName, newc2->name);

        // Add the variable maps
        RETURN_INTO_OBJREF(varmaps, cml::MapVariablesSet,
                           conn->variableMappings());
        ITERATE(varmap, MapVariable, cml::MapVariables, varmaps)
        {
            RETURN_INTO_OBJREF(newmap, cml::MapVariables,
                               mModelOut->createMapVariables());
            COPY_ATTR(newmap->firstVariableName, varmap->firstVariableName);
            COPY_ATTR(newmap->secondVariableName, varmap->secondVariableName);
            newconn->addElement(newmap);
        }
    }

    /**
     * Copy all the units in the given set into the provided model/component.
     *
     * The target is in a different model to the units, so we can't use clone.
     */
    void CopyUnits(cml::UnitsSet* unitset,
                   cml::CellMLElement* target)
    {
        RETURN_INTO_OBJREF(model, cml::Model, target->modelElement());
        // Iterate over the set
        ITERATE_S(units, Units, Units, cml::Units, unitset)
        {
            RETURN_INTO_OBJREF(units_model, cml::Model, units->modelElement());
            // Don't copy units defined in this model already
            if (units_model == model)
                continue;
            // Don't copy units we've already copied.
            // This is rather hackish, and probably not spec-compliant.
            RETURN_INTO_WSTRING(uname, units->name());
            RETURN_INTO_WSTRING(mname, units_model->name());
            std::pair<std::wstring, std::wstring> p(uname, mname);
            if (set_contains(mCopiedUnits, p))
            {
                std::wcout << "Skipped duplicate units " << uname << std::endl;
                continue;
            }
            mCopiedUnits.insert(p);
            RETURN_INTO_WSTRING(our_mname, model->name());
            std::wcout << "Copying units " << uname << "(" << units << ")"
                       << " from " << mname << "(" << units_model << ")"
                       << " to " << our_mname << "(" << model << ")"
                       << std::endl;
            RETURN_INTO_OBJREF(new_units, cml::Units, model->createUnits());
            new_units->name(uname.c_str());
            new_units->isBaseUnits(units->isBaseUnits());

            // Copy each unit reference
            ITERATE2(unit, Unit, cml::Unit, units->unitCollection())
            {
                RETURN_INTO_OBJREF(new_unit, cml::Unit, model->createUnit());
                new_unit->prefix(unit->prefix());
                new_unit->multiplier(unit->multiplier());
                new_unit->offset(unit->offset());
                new_unit->exponent(unit->exponent());
                COPY_ATTR(new_unit->units, unit->units);
                // Add
                new_units->addElement(new_unit);
            }

            // And add to target
            target->addElement(new_units);
        }
    }

    /**
     * Create and return a (manual) deep copy of the given DOM element.
     *
     * If the element is in the MathML namespace, it will be converted
     * to a cml::MathMLElement instance prior to being returned.
     *
     * Note: it appears that when adding a child element or text node,
     * which we are subsequently going to release_ref ourselves, we
     * must also release_ref the parent, or its refcount never reaches
     * zero.
     */
    dom::Element* CopyDomElement(dom::Element* in)
    {
        std::wstring nsURI = in->namespaceURI();
        std::wstring qname = in->nodeName();
        //std::wcout << "Copying DOM element " << qname;
        // Create a blank copied element
        dom::Element* out = mModelOut->createExtensionElement(nsURI.c_str(),
                                                              qname.c_str());
        ObjRef<dom::Document> doc = out->ownerDocument();

        // Copy attributes
        ObjRef<dom::NamedNodeMap> attrs = in->attributes();
        //std::wcout << " (" << attrs->length() << " attrs" << std::endl;
        for (unsigned long i=0; i<attrs->length(); ++i)
        {
            ObjRef<dom::Attr> attr = QueryInterface(attrs->item(i));
            std::wstring attr_ns = attr->namespaceURI();
            if (attr_ns == CELLML_1_1_NS)
                attr_ns = CELLML_1_0_NS;
            std::wstring attr_name = attr->name();
            //std::wcout << "qname = " << attr_name.c_str();
            dom::Attr* copy = doc->createAttributeNS(attr_ns, attr_name);
            copy->value(attr->value());
            ObjRef<dom::Attr> tmp = out->setAttributeNodeNS(copy);
            //std::wcout << " {" << copy->name() << "=" << copy->value() << "}" << std::endl;
        }

        // Copy child elements & text
        ObjRef<dom::NodeList> children = in->childNodes();
        //std::wcout << ", " << children->length() << " children)." << std::endl;
        for (unsigned long i=0; i<children->length(); ++i)
        {
            ObjRef<dom::Node> child = children->item(i);
            switch (child->nodeType())
            {
            case dom::Node::ELEMENT_NODE:
            {
                ObjRef<dom::Element> elt = QueryInterface(child);
                ObjRef<dom::Element> copy = CopyDomElement(elt);
                ObjRef<dom::Node> tmp = out->appendChild(copy);
            }
                break;
            case dom::Node::TEXT_NODE:
            {
                std::wstring text = child->nodeValue();
                // 		    size_t nonws = text.find_first_not_of(L" \t\n\r");
                // 		    if (nonws != std::string::npos)
                // 			std::wcout << "  Text: " << text << std::endl;
                ObjRef<dom::Text> textNode = doc->createTextNode(text);
                out->appendChild(textNode)->release_ref();
            }
                break;
            }
        }

        if (nsURI == MATHML_NS)
        {
            // Cast to MathML
            ObjRef<iface::mathml_dom::MathMLElement> math_out = QueryInterface(out);
            out = math_out;
        }
        // out->release_ref(); // segfaults
        return out;
    }
    
    /**
     * Create and return a (manual) deep copy of the given MathML element.
     */
    cml::MathMLElement CopyMathElement(cml::MathMLElement in)
    {
        return dynamic_cast<cml::MathMLElement>(CopyDomElement(in));
    }

    /**
     * Copy any extension elements.
     */
    void CopyExtensionElements(cml::CellMLElement* from,
                               cml::CellMLElement* to)
    {
        RETURN_INTO_OBJREF(elts, cml::ExtensionElementList,
                           from->extensionElements());
        for (unsigned long i=0; i<elts->length(); i++)
        {
            RETURN_INTO_OBJREF(elt, dom::Element, elts->getAt(i));

            // Check it's not a MathML element!
            RETURN_INTO_WSTRING(nsURI, elt->namespaceURI());
            if (nsURI == MATHML_NS)
                continue;

            RETURN_INTO_WSTRING(name, elt->nodeName());
            std::wcout << "Copying extension element " << name << std::endl;
            RETURN_INTO_OBJREF(copy, dom::Element, CopyDomElement(elt));
            to->appendExtensionElement(copy);
        }
    }

    /**
     * Copy relevant components into the output model, using the given
     * CeVAS to find components to copy.
     */
    void CopyComponents(cmlsvs::CeVAS* cevas)
    {
        ITERATE_S(comp, Component, RelevantComponents, cml::CellMLComponent, cevas)
        {
            CopyComponent(comp, mModelOut);
        }
    }

    /**
     * Copy the given component into the given model.
     *
     * We create a new component, and manually transfer the content.
     */
    void CopyComponent(cml::CellMLComponent* comp,
                       cml::Model* model)
    {
        RETURN_INTO_WSTRING(cname, comp->name());
        // Paranoia: check we haven't already copied it
        ObjRef<cml::CellMLComponent> copy = QueryInterface(mAnnoSet->getObjectAnnotation(comp, L"copy"));
        if (copy != NULL)
        {
            std::wcout << "Duplicate component " << cname << std::endl;
            return;
        }
        RETURN_INTO_OBJREF(source_model, cml::Model, comp->modelElement());
        RETURN_INTO_WSTRING(mname, source_model->name());
        std::wcout << "Copying component " << cname << " (" << comp
                   << ") from model " << mname << " (" << source_model << ")"
                   << std::endl;

        // Create the new component and set its name & id
        copy = already_AddRefd<cml::CellMLComponent>(model->createComponent());
        mAnnoSet->setObjectAnnotation(comp, L"copy", copy);
        // Check for a renaming
        RETURN_INTO_WSTRING(renamed,
                            mAnnoSet->getStringAnnotation(comp, L"renamed"));
        if (renamed.length() > 0)
        {
            // It was an imported component, and may have been renamed
            cname = renamed;
        }
        // Ensure name is unique in the 1.0 model
        EnsureComponentNameUnique(cname);
        copy->name(cname.c_str());
        RETURN_INTO_WSTRING(id, comp->cmetaId());
        if (id.length())
            copy->cmetaId(comp->cmetaId());

        // Copy units
        RETURN_INTO_OBJREF(units, cml::UnitsSet, comp->units());
        CopyUnits(units, copy);

        // Copy variables
        ITERATE2(var, Variable, cml::CellMLVariable, comp->variables())
        {
            RETURN_INTO_OBJREF(var_copy, cml::CellMLVariable,
                               model->createCellMLVariable());
            // Copy attrs
            COPY_ATTR(var_copy->name, var->name);
            RETURN_INTO_WSTRING(id, var->cmetaId());
            if (id.length())
                var_copy->cmetaId(id.c_str());
            COPY_ATTR(var_copy->initialValue, var->initialValue);
            var_copy->privateInterface(var->privateInterface());
            var_copy->publicInterface(var->publicInterface());
            COPY_ATTR(var_copy->unitsName, var->unitsName);
            copy->addElement(var_copy);
        }

        // Copy mathematics
        ITERATE_MATH(mathnode, comp->math())
        {
            cml::MathMLElement clone = CopyMathElement(mathnode);
            copy->addMath(clone);
            clone->release_ref(); // Relinquish ownership
        }

        // Copy extension elements (TODO: & attributes)
        CopyExtensionElements(comp, copy);

        // Add copy to model
        model->addElement(copy);
    }

    /**
     * This method is used to reconstruct the encapsulation hierarchy
     * in the new model, recursively processing imported models.
     *
     * It uses the CopyGroup method to read the encapsulation
     * hierarchy, and if it finds a reference to a component that has
     * been copied over, it creates a new group containing the
     * hierarchy rooted at that point.
     */
    void CopyGroups(cml::Model* model)
    {
        // Iterate only groups defining the encapsulation hierarchy.
        RETURN_INTO_OBJREF(groups, cml::GroupSet, model->groups());
        ITERATE2(group, Group, cml::Group, groups->subsetInvolvingEncapsulation())
        {
            // Now recurse down this subtree
            RETURN_INTO_OBJREF(crefs, cml::ComponentRefSet,
                               group->componentRefs());
            CopyGroup(model, crefs, NULL);
        }

        // Now check imported models
        ITERATE2(import, Import, cml::CellMLImport, model->imports())
        {
            RETURN_INTO_OBJREF(imp_model, cml::Model, import->importedModel());
            assert(imp_model != NULL);
            CopyGroups(imp_model);
        }
    }

    /**
     * This method does the actual copying of groups.
     */
    void CopyGroup(cml::Model* model,
                   cml::ComponentRefSet* crefs,
                   cml::ComponentRef* copyInto)
    {
        RETURN_INTO_OBJREF(comps, cml::CellMLComponentSet,
                           model->modelComponents());

        // Iterate this level of the tree
        ITERATE(cref, ComponentRef, cml::ComponentRef, crefs)
        {
            // Find the referenced component
            RETURN_INTO_WSTRING(cname, cref->componentName());
            RETURN_INTO_OBJREF(comp, cml::CellMLComponent,
                               comps->getComponent(cname.c_str()));
            if (comp == NULL)
            {
                RETURN_INTO_WSTRING(mname, model->name());
                std::wcout << "Component " << cname << " referred to in the "
                           << "encapsulation hierarchy of model " << mname
                           << " does not exist." << std::endl;
                continue;
            }
            // Find the real component object
            RETURN_INTO_OBJREF(real_comp, cml::CellMLComponent,
                               FindRealComponent(comp));
            // Has it been copied?
            ObjRef<cml::CellMLComponent> copy = QueryInterface(mAnnoSet->getObjectAnnotation(real_comp, L"copy"));
            if (copy == NULL)
            {
                if (copyInto != NULL)
                {
                    RETURN_INTO_WSTRING(mname, model->name());
                    std::wcout << "Component " << cname << " in model "
                               << mname << " had its encapsulation parent copied,"
                               << " but wasn't copied itself." << std::endl;
                }
                continue;
            }

            // Create a component ref for the copy
            RETURN_INTO_OBJREF(newref, cml::ComponentRef,
                               mModelOut->createComponentRef());
            COPY_ATTR(newref->componentName, copy->name);

            if (copyInto == NULL)
            {
                // Create a new group
                RETURN_INTO_OBJREF(group, cml::Group, mModelOut->createGroup());
                mModelOut->addElement(group);
                RETURN_INTO_OBJREF(rref, cml::RelationshipRef,
                                   mModelOut->createRelationshipRef());
                rref->setRelationshipName(L"", L"encapsulation");
                group->addElement(rref);

                // Add this component as the root
                group->addElement(newref);
            }
            else
            {
                // Add this component into the existing group
                copyInto->addElement(newref);
            }

            // Copy any children of this component
            RETURN_INTO_OBJREF(childrefs, cml::ComponentRefSet,
                               cref->componentRefs());
            CopyGroup(model, childrefs, newref);
        }
    }

    /**
     * Try to make all initial_value attributes valid CellML 1.0.
     *
     * Where a variable is specified, look at its source variable.  If
     * it has a numeric initial value, use that.  If not, it's an
     * unavoidable error condition.
     */
    void PropagateInitialValues()
    {
        ITERATE2(comp, Component, cml::CellMLComponent,
                 mModelOut->localComponents())
        {
            RETURN_INTO_WSTRING(cname, comp->name());
            ITERATE2(var, Variable, cml::CellMLVariable, comp->variables())
            {
                RETURN_INTO_WSTRING(init, var->initialValue());
                if (init.length() == 0)
                    continue;
                // Is the initial value a double?
                std::wstringstream init_(init);
                double value;
                init_ >> value;
                if (init_.fail()) {
                    RETURN_INTO_WSTRING(vname, var->name());
                    std::wcout << "Var " << cname << ":" << vname
                               << " has initvar " << init;
                    // Find the initial variable
                    RETURN_INTO_OBJREF(vars, cml::CellMLVariableSet,
                                       comp->variables());
                    RETURN_INTO_OBJREF(initvar, cml::CellMLVariable,
                                       vars->getVariable(init.c_str()));
                    // Find its source
                    RETURN_INTO_OBJREF(src, cml::CellMLVariable,
                                       initvar->sourceVariable());
                    // And copy the initial value
                    RETURN_INTO_WSTRING(srcinit, src->initialValue());
                    var->initialValue(srcinit.c_str());
                    std::wcout << " value " << srcinit << std::endl;
                }
            }
        }
    }

public:
    /**
     * The main interface to the converter: creates and returns a new
     * model which is a CellML 1.0 version of the input.
     *
     * The input model must have had all imports fully instantiated.
     *
     * @return the converted model, using the 1.0 namespace.
     */
    cml::Model* ConvertModel(cml::Model* modelIn)
    {
        RETURN_INTO_WSTRING(model_name, modelIn->name());
        std::wcout << "Converting model " << model_name << " to CellML 1.0."
                   << std::endl;
        Reset();
        mModelIn = modelIn;

        // Create the output model
        RETURN_INTO_OBJREF(cbs, cml::CellMLBootstrap,
                           CreateCellMLBootstrap());
        mModelOut = cbs->createModel(L"1.0");

        // Set name & id
        mModelOut->name(model_name.c_str());
        RETURN_INTO_WSTRING(model_id, modelIn->cmetaId());
        if (model_id.length() > 0)
            mModelOut->cmetaId(model_id.c_str());

        // Create an annotation set to manage annotations
        RETURN_INTO_OBJREF(ats, cmlsvs::AnnotationToolService,
                           CreateAnnotationToolService());
        RETURN_INTO_OBJREF(annoset, cmlsvs::AnnotationSet,
                           ats->createAnnotationSet());
        mAnnoSet = annoset;

        // Create a CeVAS to find relevant components
        RETURN_INTO_OBJREF(cevas_bs, cmlsvs::CeVASBootstrap,
                           CreateCeVASBootstrap());
        RETURN_INTO_OBJREF(cevas, cmlsvs::CeVAS,
                           cevas_bs->createCeVASForModel(modelIn));
        RETURN_INTO_WSTRING(err, cevas->modelError());
        if (err.length() > 0)
        {
            std::wcerr << "Error creating CeVAS: " << err << std::endl;
            return NULL;
        }

        // Copy model-level units to the new model, both local and
        // imported definitions.
        RETURN_INTO_OBJREF(units, cml::UnitsSet, modelIn->allUnits());
        CopyUnits(units, mModelOut);

        // Annotate potentially renamed components
        StoreImportRenamings(modelIn);

        // Copy all needed components to the new model
        CopyComponents(cevas);

        // Copy connections
        CopyConnections(modelIn);

        // Copy groups
        CopyGroups(modelIn);

        // Deal with 'initial_value="var_name"' occurrences
        PropagateInitialValues();

        // And finally, return the result
        return mModelOut;
    }
};

ObjRef<cml::Model> flattenModel(cml::Model* model)
{
    ObjRef<cml::Model> new_model;
    {
        VersionConverter converter;
        new_model = converter.ConvertModel(model);
    }
    return new_model;
}

