#pragma once

#include "turtle335/model/ModelPackage.h"

#include <filesystem>
#include <vector>


namespace turtle335
{


class ModelScanner
{

public:


    std::vector<ModelPackage> Scan(
        const std::filesystem::path& root
    );


private:


    ModelPackage ScanDirectory(
        const std::filesystem::path& directory
    );


};


}