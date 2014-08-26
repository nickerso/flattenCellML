#ifndef XMLUTILS_HPP
#define XMLUTILS_HPP

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
     * Checks the current XML document to see if the named variable can be found in a simple LHS assignment
     * of an algebraic expression. e.g., vname = a * x + b
     * @param vname The name of the variable to search for.
     * @return The MathML string containing the matching equation, if one is found. Otherwise the empty string
     * is returned.
     */
    std::wstring matchAlgebraicLhs(const std::wstring& vname);

    /**
     * The current document is expected to be a simple MathML numerical assignment, and this function will return
     * the numerical value being assigned and the units specified in the math.
     * @param value The numerical value being assigned.
     * @param unitsName The name of the units specified on the numerical value (an error if no units present).
     * @return zero on success, non-zero on error. A return value of -1 indicates missing units.
     */
    int numericalAssignmentGetValue(double* value, std::wstring& unitsName);

private:
    void* mCurrentDoc;

    std::string getTextContent(const char* xpathExpr);
    int getDoubleContent(const char* xpathExpr, double* value);
};

#endif // XMLUTILS_HPP
