#include <map>
#include <string>
#include <cstring>
#include <iostream>
#include <vector>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "xmlutils.hpp"
#include "utils.hpp"

#define MATHML_NS "http://www.w3.org/1998/Math/MathML"
#define CELLML_1_0_NS "http://www.cellml.org/cellml/1.0#"
#define CELLML_1_1_NS "http://www.cellml.org/cellml/1.1#"


static xmlNodeSetPtr executeXPath(xmlDocPtr doc, const xmlChar* xpathExpr)
{
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlNodeSetPtr results = NULL;
    //std::cout << "executing xPath: " << xpathExpr << "; on the document: ===>";
    //xmlDocDump(stdout, doc);
    //std::cout << "<===" << std::endl;
    /* Create xpath evaluation context */
    xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL)
    {
        fprintf(stderr, "Error: unable to create new XPath context\n");
        return NULL;
    }
    /* Register namespaces */
    if ((xmlXPathRegisterNs(xpathCtx, BAD_CAST "mathml", BAD_CAST MATHML_NS) != 0) ||
        (xmlXPathRegisterNs(xpathCtx, BAD_CAST "cellml10", BAD_CAST CELLML_1_0_NS) != 0) ||
        (xmlXPathRegisterNs(xpathCtx, BAD_CAST "cellml11", BAD_CAST CELLML_1_1_NS) != 0))
    {
        std::cerr << "ERROR registering namespaces?" << std::endl;
        return NULL;
    }
    xmlXPathRegisterAllFunctions(xpathCtx);

    /* Evaluate xpath expression */
    xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if (xpathObj == NULL)
    {
        fprintf(stderr, "Error: unable to evaluate xpath expression \"%s\"\n",
                xpathExpr);
        xmlXPathFreeContext(xpathCtx);
        return NULL;
    }
    //std::cout << "xpath object type = " << xpathObj->type << std::endl;
    if (xmlXPathNodeSetGetLength(xpathObj->nodesetval) > 0)
    {
        int i;
        results = xmlXPathNodeSetCreate(NULL);
        for (i=0; i< xmlXPathNodeSetGetLength(xpathObj->nodesetval); ++i)
            xmlXPathNodeSetAdd(results, xmlXPathNodeSetItem(xpathObj->nodesetval, i));
    }
    //else std::wcout << L"No nodesetval?" << std::endl;
    /* Cleanup */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    return results;
}

static std::wstring nodeToString(xmlNodePtr node)
{
    std::wstring nodeString;
    // Need to make sure we have the namespace declarations included in the string - this seems
    // to work?!
    xmlDocPtr newDoc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr newNode = xmlDocCopyNode(node, newDoc, 1);
    xmlBufferPtr buf = xmlBufferCreate();
    if (xmlNodeDump(buf, newDoc, newNode, 0, 1) > 0)
    {
        std::string text = std::string((char*)xmlBufferContent(buf));
        nodeString = string2wstring(text);
        //std::wcout << L"Found text: " << eq << std::endl;
    }
    else
    {
        std::cerr << "ERROR: unable to get node contents?" << std::endl;
    }
    return nodeString;
}

class LibXMLWrapper
{
public:
    LibXMLWrapper()
    {
        std::cout << "initialise libxml\n";
        /* Init libxml */
        xmlInitParser();
        LIBXML_TEST_VERSION
    }
    ~LibXMLWrapper()
    {
        std::cout << "terminate libxml\n";
        /* Shutdown libxml */
        xmlCleanupParser();
    }
};

static LibXMLWrapper dummyWrapper;

XmlUtils::XmlUtils() : mCurrentDoc(0)
{
}

XmlUtils::~XmlUtils()
{
    if (mCurrentDoc)
    {
        xmlFreeDoc(static_cast<xmlDocPtr>(mCurrentDoc));
    }
}

int XmlUtils::parseString(const std::wstring &data)
{
    if (mCurrentDoc)
    {
        xmlFreeDoc(static_cast<xmlDocPtr>(mCurrentDoc));
    }
    std::string s = wstring2string(data);
    xmlDocPtr doc = xmlParseMemory(s.c_str(), s.size());
    if (doc == NULL)
    {
        std::cerr << "Error parsing data string: **" << s.c_str() << "**\n";
        return -1;
    }
    mCurrentDoc = static_cast<void*>(doc);
    return 0;
}

std::wstring XmlUtils::serialise(int format)
{
    if (mCurrentDoc == 0)
    {
        std::cerr << L"Trying to serialise nothing?\n";
        return L"";
    }
    xmlDocPtr doc = static_cast<xmlDocPtr>(mCurrentDoc);
    xmlChar* data;
    int size = -1;
    xmlDocDumpFormatMemory(doc, &data, &size, format);
    std::string xmlString((char*)data);
    std::wstring xs = string2wstring(xmlString);
    xmlFree(data);
    return xs;
}

std::wstring XmlUtils::matchConstantParameterEquation(const std::wstring &vname)
{
    std::wstring eq;
    std::string xpath = "/mathml:math/mathml:apply/mathml:eq/following-sibling::mathml:ci[normalize-space(text()) = \"";
    xpath += wstring2string(vname);
    xpath += "\"]/following-sibling::mathml:cn/parent::mathml:apply";
    //std::cout << "XPath expression: &&" << xpath << "$$" << std::endl;
    xmlDocPtr doc = static_cast<xmlDocPtr>(mCurrentDoc);
    xmlNodeSetPtr results = executeXPath(doc, BAD_CAST xpath.c_str());
    if (results)
    {
        if (xmlXPathNodeSetGetLength(results) == 1)
        {
            xmlNodePtr n = xmlXPathNodeSetItem(results, 0);
            eq = nodeToString(n);
        }
        //else std::wcout << L"Not 1 result?" << std::endl;
        xmlXPathFreeNodeSet(results);
    }
    //else std::wcout << L"No results" << std::endl;
    return eq;
}

std::wstring XmlUtils::matchSimpleEquality(const std::wstring &vname)
{
    std::wstring eq;
    std::string xpath = "/mathml:math/mathml:apply/mathml:eq/following-sibling::mathml:ci[normalize-space(text()) = \"";
    xpath += wstring2string(vname);
    xpath += "\"]/following-sibling::mathml:ci/parent::mathml:apply";
    //std::cout << "XPath expression: &&" << xpath << "$$" << std::endl;
    xmlDocPtr doc = static_cast<xmlDocPtr>(mCurrentDoc);
    xmlNodeSetPtr results = executeXPath(doc, BAD_CAST xpath.c_str());
    if (results)
    {
        if (xmlXPathNodeSetGetLength(results) == 1)
        {
            xmlNodePtr n = xmlXPathNodeSetItem(results, 0);
            eq = nodeToString(n);
        }
        //else std::wcout << L"Not 1 result?" << std::endl;
        xmlXPathFreeNodeSet(results);
    }
    //else std::wcout << L"No results" << std::endl;
    return eq;
}

std::wstring XmlUtils::matchAlgebraicLhs(const std::wstring &vname)
{
    std::wstring eq;
    return eq;
}

int XmlUtils::numericalAssignmentGetValue(double *value, std::wstring &unitsName)
{
    int returnCode = 0;
    std::string xpath = "/mathml:apply/mathml:cn";
    std::cout << "XPath expression: &&" << xpath << "$$" << std::endl;
    returnCode = getDoubleContent(xpath.c_str(), value);
    if (returnCode != 0) return -2;
    std::cout << "got a double value: " << *value << std::endl;
    xpath = "/mathml:apply/mathml:cn/@cellml11:units";
    unitsName = string2wstring(getTextContent(xpath.c_str()));
    if (unitsName.empty())
    {
        xpath = "/mathml:apply/mathml:cn/@cellml10:units";
        unitsName = string2wstring(getTextContent(xpath.c_str()));
        if (unitsName.empty())
        {
            std::cerr << "Unable to find a units attribute?" << std::endl;
            return -1;
        }
    }
    std::wcout << L"Found units: " << unitsName << std::endl;
    return returnCode;
}

std::string XmlUtils::getTextContent(const char* xpathExpr)
{
    std::string text;
    xmlDocPtr doc = static_cast<xmlDocPtr>(mCurrentDoc);
    xmlNodeSetPtr results = executeXPath(doc, BAD_CAST xpathExpr);
    if (results)
    {
        if (xmlXPathNodeSetGetLength(results) == 1)
        {
            xmlNodePtr n = xmlXPathNodeSetItem(results, 0);
            xmlChar* s = xmlNodeGetContent(n);
            text = std::string((char*)s);
            xmlFree(s);
        }
        xmlXPathFreeNodeSet(results);
    }
    else
    {
        std::cerr << "ERROR: no results found for the XPath expression: " << xpathExpr
                  << "; with the document: " << std::endl;
        xmlDocDump(stderr, doc);
    }
    return text;
}

int XmlUtils::getDoubleContent(const char* xpathExpr, double* value)
{
    int returnCode = -1;
    std::string textContent = getTextContent(xpathExpr);
    if (! textContent.empty())
    {
        if (sscanf(textContent.c_str(), "%lf", value) == 1) returnCode = 0;
        else std::cerr << "getDoubleContent: found a value for xpath expression, but its not a number: \""
                       << textContent << "\"" << std::endl;
    }
    return returnCode;
}
