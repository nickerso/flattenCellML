
#include <string>
#include <iostream>
#include <fstream>

#include <IfaceCellML_APISPEC.hxx>
#include <cellml-api-cxx-support.hpp>
#include <CellMLBootstrap.hpp>

#include "utils.hpp"
#include "VersionConverter.hpp"
#include "ModelCompactor.hpp"
#include "compactorreport.hpp"

// Save typing
namespace cml = iface::cellml_api;

static void usage(const char* progName)
{
    std::cerr << "Usage: " << progName << " <model | variables> <modelURL> [output file]" << std::endl;
    std::cerr << "The first argument defines the flattening mode.\n";
    std::cerr << "  model:      flattens the model maintaining the modular structure.\n";
    std::cerr << "  variables:  create a single component defining all the variables\n"
                 "              specified in the top level of the given model.\n";
    std::cerr << std::endl;
}

int main(int argc, char* argv[])
{
    // We should have a model URI in argv[2]
    if (argc < 3)
    {
        usage(argv[0]);
        return -1;
    }
    std::string mode(argv[1]);
    std::wstring model_url = string2wstring(argv[2]);
    const char* output_file_name = NULL;
    if (argc == 4)
    {
        output_file_name = argv[3];
    }
    if (!((mode == "model") || (mode == "variables")))
    {
        std::cerr << "A flattening mode of either \"model\" or \"variables\" is required." << std::endl;
        usage(argv[0]);
        return -2;
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
        if (mode == "model") model->fullyInstantiateImports(); // Make sure we have all of it
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
    std::wcout << "Loaded model '" << model_name.c_str() << "' id '" << model_id.c_str();
    if (mode == "model") std::wcout << "' with all imports." << std::endl;
    else std::wcout << "'." << std::endl;

    // Now we can do stuff
    CompactorReport report;
    report.setIndentString(L"+== ");
    report.setIndentLevel(0);
    ObjRef<cml::Model> new_model;
    if (mode == "model") new_model = flattenModel(model);
    else new_model = compactModel(model, report);

    if (new_model == NULL)
    {
        std::cerr << "Something went wrong!" << std::endl;
        std::wstring reportString = report.getReport();
        if (! reportString.empty()) std::wcout << reportString << std::endl;
        return 2;
    }

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

    std::wstring reportString = report.getReport();
    if (! reportString.empty()) std::wcout << reportString << std::endl;

    // and exit
    return 0;
}
