#include <iostream>

#include "preprocess.h"

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cerr << "Usage : " << argv[0] << " <source-file>" << std::endl;
        return 1;
    }

    std::string code = toycc::preprocess(argv[1]);

    std::cout << code << std::endl;
    return 0;
}
