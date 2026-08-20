#include "turtle335/adt/AdtValidator.h"

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
            std::cerr << "Usage: turtle335_validate_adt <file.adt>\n";
            return 2;
        }

        const std::string path = argv[1];
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("cannot open ADT: " + path);

        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        if (!in.eof() && in.fail())
            throw std::runtime_error("failed while reading ADT: " + path);

        const AdtValidationReport report = ValidateVanillaAdt(bytes);
        std::cout << "ADT validation: PASS\n"
                  << "file: " << path << '\n'
                  << "bytes: " << report.fileSize << '\n'
                  << "version: " << report.version << '\n'
                  << "textures: " << report.textureCount << '\n'
                  << "M2 paths/placements: " << report.m2PathCount << '/' << report.m2PlacementCount << '\n'
                  << "WMO paths/placements: " << report.wmoPathCount << '/' << report.wmoPlacementCount << '\n'
                  << "MCNK: " << report.mcnkCount << '\n'
                  << "liquid records: " << report.liquidRecordCount << '\n'
                  << "sound emitters: " << report.soundEmitterCount << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ADT validation: FAIL\n" << e.what() << '\n';
        return 1;
    }
}
