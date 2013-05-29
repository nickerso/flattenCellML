
#include <string>
#include <iostream>
#include <fstream>

#include <IfaceCellML_APISPEC.hxx>
#include <cellml-api-cxx-support.hpp>
#include <CellMLBootstrap.hpp>

#include "utils.hpp"
#include "VersionConverter.hpp"

// Save typing
namespace cml = iface::cellml_api;

int main(int argc, char* argv[])
{
    // We should have a model URI in argv[1]
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <modelURL> [output file]" << std::endl;
        return -1;
    }
    std::wstring model_url = string2wstring(argv[1]);
    const char* output_file_name = NULL;
    if (argc == 3)
    {
        output_file_name = argv[2];
    }

    // Bootstrap the API
    ObjRef<cml::CellMLBootstrap> cbs = CreateCellMLBootstrap();
    // Get a model loader
    ObjRef<cml::DOMModelLoader> ml = cbs->modelLoader();
    // Load the model
    ObjRef<cml::Model> model;
    try
    {
        model = ml->loadFromURL(model_url.c_str());
        model->fullyInstantiateImports(); // Make sure we have all of it
    }
    catch (cml::CellMLException& e)
    {
        // Work around CORBA deficiencies to get the error message
        std::wstring msg = ml->lastErrorMessage();
        printf("Error loading model: %S\n", msg.c_str());
        return 1;
    }
    // Print the model's name & id to indicate successful load
    std::wstring model_id = model->cmetaId();
    std::wstring model_name = model->name();
    std::wcout << "Loaded model '" << model_name.c_str() << "' id '" << model_id.c_str()
               << "' with all imports." << std::endl;

    // Now we can do stuff
    ObjRef<cml::Model> new_model = flattenModel(model);

    // Print the model to file
    std::wstring content = new_model->serialisedText();
    if (output_file_name != NULL)
    {
        std::wofstream out(output_file_name);
        out << content.c_str();
        if (out.fail())
        {
            std::wcerr << "Failed to write to given output file" << std::endl;
        }
        out.close();
    }
    else
    {
        // Write to stdout
        std::wcout << content.c_str();
    }

    // and exit
    return 0;
}
