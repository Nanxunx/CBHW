#include "turtle335/m2/ClassicM2Validator.h"

#include <fstream>
#include <iostream>
#include <vector>


int main(int argc,char** argv)
{
    if(argc < 2)
    {
        std::cout
            << "usage: turtle335_validate_m2 <file.m2>\n";
        return 1;
    }


    std::ifstream file(
        argv[1],
        std::ios::binary
    );


    if(!file)
    {
        std::cerr
            << "cannot open "
            << argv[1]
            << "\n";
        return 1;
    }


    std::vector<unsigned char> data(
        std::istreambuf_iterator<char>(file),
        {}
    );


    auto result =
        turtle335::m2::ValidateClassicM2(
            data
        );


    std::cout
        << "file="
        << argv[1]
        << "\n";


    std::cout
        << "bytes="
        << data.size()
        << "\n";


    std::cout
        << "valid="
        << (result.valid ? "true":"false")
        << "\n";


    for(auto& issue : result.issues)
    {
        std::cout
            << issue.code
            << ": "
            << issue.detail
            << "\n";
    }


    return result.valid ? 0:2;
}