flattenCellmlModel
==================

Code to flatten CellML 1.1 models into CellML 1.0 models.

Two model flattening algorithms are available:

1. using an updated version of Jonathan Cooper's [VersionConverter] (http://www.cellml.org/tools/jonathan-cooper-s-cellml-1-1-to-1-0-converter/versionconverter-tar.bz2/view) code.

    Uses the CellML DOM API to flatten modular CellML 1.1 models into a single document CellML 1.0 model. This is a slightly updated version of Jonathan Cooper's original VersionConverter code which is known to work with version 1.12 of the CellML API.

2. using a new (and in-development) model compaction algorithm.

    Trying to address some of the unsupported features of the VersionConverter class and produce an accurate representation of the mathematical model without worrying about the modularity. Units will all be converted to their canonical representation in the generated model.

