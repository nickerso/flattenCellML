#ifndef XMLUTILS_HPP
#define XMLUTILS_HPP

#include <utility>
#include <string>
#include <vector>

class XmlUtils
{
public:
    XmlUtils();
    ~XmlUtils();

    int parseString(const std::wstring &data);
    std::wstring serialise(int format = 1);

    /**
     * Checks the current XML document to see if a constant parameter equation can be found for the given
     * variable name. e.g., vname = 1.23 [ms]
     * @param vname The variable name to search for.
     * @return The MathML string containing the matching simple assignment equation, if one is found.
     * Otherwise the empty string is returned.
     */
    std::wstring matchConstantParameterEquation(const std::wstring& vname);

    /**
     * Checks the current XML document to see if a simple variable equality equation can be found for the
     * given variable name. e.g., vname = otherVariable
     * @param vname The variable name to search for.
     * @return The MathML string containing the matching simple equality equation, if one is found. Otherwise
     * the empty string will be returned.
     */
    std::wstring matchSimpleEquality(const std::wstring& vname);

    /**
     * Checks the current XML document to see if the named variable can be found in a simple LHS assignment
     * of an algebraic expression. e.g., vname = a * x + b
     * @param vname The name of the variable to search for.
     * @return The MathML string containing the matching equation, if one is found. Otherwise the empty string
     * is returned.
     */
    std::wstring matchAlgebraicLhs(const std::wstring& vname);

    /**
     * Checks the current XML document to see if the named variable can be found in a differential equation LHS.
     * e.g., d(vname)/d(time) = ...
     * @param vname The name of the variable to search for.
     * @return The MathML string containing the matching equation, if one is found. Otherwise the empty string
     * is returned.
     */
    std::wstring matchDifferential(const std::wstring& vname);

    /**
     * Returns true if the given variable matches the pattern for a variable of integration.
     * @param vname The name of the variable to search for.
     * @return true if the given variable is a variable of integration, false otherwise.
     */
    bool matchVariableOfIntegration(const std::wstring &vname);

    /**
     * The current document is expected to be a simple MathML numerical assignment, and this function will return
     * the numerical value being assigned and the units specified in the math.
     * @param value The numerical value being assigned.
     * @param unitsName The name of the units specified on the numerical value (an error if no units present).
     * @return zero on success, non-zero on error. A return value of -1 indicates missing units.
     */
    int numericalAssignmentGetValue(double* value, std::wstring& unitsName);

    /**
     * The current document is expected to be a simple MathML variable equality, and this function will return the
     * two variable names.
     * @return The pair of variable names in the equality relationship.
     */
    std::pair<std::wstring, std::wstring> simpleEqualityGetVariableNames();

    /**
     * Find all the CellML variable names used in the current MathML document.
     * @return A list of all the variable names found in the current document. Each name will only appear once.
     */
    std::vector<std::wstring> getCiList();

    /**
     * Update all the ci elements in the current MathML document with the given name mappings.
     * @param nameMapping The mapping from existing name to a new name.
     * @return The updated MathML document string.
     */
    std::wstring updateCiElements(const std::map<std::wstring, std::wstring>& nameMapping);

private:
    void* mCurrentDoc;

    std::string getTextContent(const char* xpathExpr);
    int getDoubleContent(const char* xpathExpr, double* value);

    /**
     * Look for a single node in the current XML doc which matches the given xpathExpr. If one is found, serialise it
     * and return the serialisation.
     * @param xpathExpr The XPath expression to execute on the current document.
     * @return If executing the XPath expression on the current document results in a single result node, serialise it
     * to a string and return it. Otherwise, return the empty string.
     */
    std::wstring extractSingleNode(const std::string& xpathExpr);
};

#endif // XMLUTILS_HPP
