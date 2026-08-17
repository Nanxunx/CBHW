#include "turtle335/adt/WdtWriter.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace turtle335::adt;

int main(int argc, char** argv)
{
    try
    {
        const std::string outputPath = argc >= 2 ? argv[1] : "FixtureA.wdt";
        WdtWriterInput input;
        SetWdtTerrainTile(input, 32, 32, true);
        const auto bytes = SerializeTerrainWdt(input);
        const auto report = ValidateTerrainWdt(bytes);
        if (report.presentTiles != 1)
            throw std::runtime_error("generated WDT did not retain exactly one terrain tile");

        std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("cannot create output WDT: " + outputPath);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out)
            throw std::runtime_error("failed while writing output WDT: " + outputPath);

        std::cout << "Fixture A WDT written: " << outputPath
                  << " (" << bytes.size() << " bytes, present tile 32,32)\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fixture A WDT generation failed: " << e.what() << '\n';
        return 1;
    }
}
