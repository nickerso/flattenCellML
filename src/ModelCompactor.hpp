#ifndef MODELCOMPACTOR_HPP
#define MODELCOMPACTOR_HPP

#include <IfaceCellML_APISPEC.hxx>
#include <cellml-api-cxx-support.hpp>

/**
 * Compact the given model into a model which contains just two components. One component will define all
 * variables found in the top-level of the given model, and the other component will contain all the variables
 * and math required to fully define those variables. As a by-product of this compaction, all units will be
 * converted to their cononical representation.
 * @param model The source model to compact (imports will be instantiated when needed).
 * @return If compaction is successful, returns the compacted model. Otherwise NULL on failure.
 */
ObjRef<iface::cellml_api::Model> compactModel(iface::cellml_api::Model* model);

#endif // MODELCOMPACTOR_HPP
