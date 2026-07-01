// xerces-c++ grammar deserialization trigger
// uses only public xerces APIs. the vulnerable code is in libxerces-c-4.0.so

#include <xercesc/framework/XMLGrammarPoolImpl.hpp>
#include <xercesc/util/BinFileInputStream.hpp>
#include <xercesc/util/PlatformUtils.hpp>

#include <cstdio>

using namespace xercesc;

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <grammar.bin>\n", argv[0]);
        return 2;
    }

    XMLPlatformUtils::Initialize();
    {
        XMLGrammarPoolImpl pool;
        BinFileInputStream in(argv[1]);
        try {
            pool.deserializeGrammars(&in);
            fprintf(stderr, "deserialized\n");
        } catch (...) {
        }
    }
    XMLPlatformUtils::Terminate();
    return 0;
}
