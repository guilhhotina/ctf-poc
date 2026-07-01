#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/XMLGrammarPoolImpl.hpp>
#include <xercesc/internal/BinFileOutputStream.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/validators/common/Grammar.hpp>

#include <iostream>
#include <sstream>
#include <string>

using namespace xercesc;

int main() {
    XMLPlatformUtils::Initialize();

    {
        XMLGrammarPoolImpl pool;
        XercesDOMParser parser(0, XMLPlatformUtils::fgMemoryManager, &pool);
        parser.setValidationScheme(XercesDOMParser::Val_Always);
        parser.setDoNamespaces(true);
        parser.setDoSchema(true);
        parser.cacheGrammarFromParse(true);

        std::string longName(48, 'L');
        std::ostringstream xsd;
        xsd << "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n";
        xsd << "  <xs:element name=\"AAAAAAAA\">\n";
        xsd << "    <xs:complexType>\n";
        xsd << "      <xs:attribute name=\"attr0000000\" type=\"xs:string\"/>\n";
        xsd << "      <xs:attribute name=\"attr1111111\" type=\"xs:string\"/>\n";
        xsd << "    </xs:complexType>\n";
        xsd << "  </xs:element>\n";
        xsd << "  <xs:element name=\"" << longName << "\" type=\"xs:string\"/>\n";
        xsd << "  <xs:attribute name=\"globalAttr\" type=\"xs:string\"/>\n";
        xsd << "</xs:schema>\n";

        std::string schema = xsd.str();
        MemBufInputSource src((const XMLByte*)schema.data(), schema.size(), "uaf3.xsd", false);
        parser.loadGrammar(src, Grammar::SchemaGrammarType, true);

        BinFileOutputStream out("uaf3_grammar.bin");
        pool.serializeGrammars(&out);
    }
    XMLPlatformUtils::Terminate();
    return 0;
}
