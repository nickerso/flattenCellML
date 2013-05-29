#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <string>

#include <IfaceCellML_APISPEC.hxx>
#include <CellMLBootstrap.hpp>
#include <cellml-api-cxx-support.hpp>


std::string wstring2string(const wchar_t* str)
{
    if (str)
    {
        size_t len = wcsrtombs(NULL,&str,0,NULL);
        if (len > 0)
        {
            len++;
            char* s = (char*)malloc(len);
            wcsrtombs(s,&str,len,NULL);
            std::string st = std::string(s);
            free(s);
            return(st);
        }
    }
    return("");
}

std::wstring string2wstring(const char* str)
{
    if (str)
    {
        wchar_t* s;
        size_t l = strlen(str);
        s = (wchar_t*)malloc(sizeof(wchar_t)*(l+1));
        memset(s,0,(l+1)*sizeof(wchar_t));
        mbsrtowcs(s,&str,l,NULL);
        std::wstring st = std::wstring(s);
        free(s);
        return(st);
    }
    return(L"");
}

std::string getCellMLMetadataAsRDFXMLString(const char* mbrurl)
{
    std::string s;

    // Get the URL from which to load the model
    if (!mbrurl) return(s);
    std::wstring URL = string2wstring(mbrurl);

    RETURN_INTO_OBJREF(cb, iface::cellml_api::CellMLBootstrap, CreateCellMLBootstrap());
    RETURN_INTO_OBJREF(ml, iface::cellml_api::ModelLoader, cb->modelLoader());
    // Try and load the CellML model from the URL
    try
    {
        RETURN_INTO_OBJREF(model, iface::cellml_api::Model, ml->loadFromURL(URL.c_str()));
        RETURN_INTO_OBJREF(rr, iface::cellml_api::RDFRepresentation,
                           model->getRDFRepresentation(L"http://www.cellml.org/RDFXML/string"));
        if (rr)
        {
            DECLARE_QUERY_INTERFACE(rrs,rr,cellml_api::RDFXMLStringRepresentation);
            std::wstring rdf = rrs->serialisedData();
            s = wstring2string(rdf.c_str());
        }
    }
    catch (...)
    {
        std::cerr << "getCellMLMetadataAsRDFXMLString: Error loading model URL: " << mbrurl << std::endl;
        return(s);
    }
    return(s);
}
