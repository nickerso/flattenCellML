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
     * Checks the current XML document to see if a simple assignment can be found for the given
     * variable name. e.g., vname = 1.23 [ms]
     * @param vname The variable name to search for.
     * @return The MathML string containing the matching simple assignment equation, if one is found.
     * Otherwise the empty string is returned.
     */
    std::wstring matchSimpleAssignment(const std::wstring& vname);

    /**
     * Checks the current XML document to see if the named variable can be found in a simple LHS assignment
     * of an algebraic expression. e.g., vname = a * x + b
     * @param vname The name of the variable to search for.
     * @return The MathML string containing the matching equation, if one is found. Otherwise the empty string
     * is returned.
     */
    std::wstring matchAlgebraicLhs(const std::wstring& vname);

private:
    void* mCurrentDoc;
};

#endif // XMLUTILS_HPP
