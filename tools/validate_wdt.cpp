#include "turtle335/adt/WdtWriter.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace turtle335::adt;

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: turtle335_validate_wdt <file.wdt>\n";
            return 2;
        }

        const std::string path = argv[1];
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("cannot open WDT: " + path);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        if (!in.eof() && in.fail())
            throw std::runtime_error("failed while reading WDT: " + path);

        const auto report = ValidateTerrainWdt(bytes);
        std::cout << "WDT validation: PASS\n"
                  << "file: " << path << '\n'
                  << "bytes: " << report.fileSize << '\n'
                  << "version: " << report.version << '\n'
                  << "MPHD flags: 0x" << std::hex << report.headerFlags << std::dec << '\n'
                  << "present terrain tiles: " << report.presentTiles << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "WDT validation: FAIL\n" << e.what() << '\n';
        return 1;
    }
}
