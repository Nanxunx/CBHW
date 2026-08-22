#include "turtle335/model/ModelScanner.h"

namespace turtle335
{


std::vector<ModelPackage>
ModelScanner::Scan(
    const std::filesystem::path& root
)
{
    std::vector<ModelPackage> result;


    for(auto& entry :
        std::filesystem::directory_iterator(root))
    {

        if(entry.is_directory())
        {

            auto package =
                ScanDirectory(
                    entry.path()
                );


            if(package.IsValid())
            {
                result.push_back(
                    package
                );
            }

        }

    }


    return result;
}



ModelPackage
ModelScanner::ScanDirectory(
    const std::filesystem::path& directory
)
{

    ModelPackage package;


    package.name =
        directory.filename().string();



    for(auto& file :
        std::filesystem::directory_iterator(directory))
    {

        auto ext =
            file.path().extension().string();



        auto path =
            file.path().string();



        if(
            ext == ".M2" ||
            ext == ".m2"
        )
        {

            package.m2Files.push_back(
                file.path().string()
            );

        }


        else if(
            ext == ".skin"
        )
        {

            package.skins.push_back(
                path
            );

        }


        else if(
            ext == ".anim"
        )
        {

            package.animations.push_back(
                path
            );

        }


        else if(
            ext == ".blp"
        )
        {

            package.textures.push_back(
                path
            );

        }

    }


    return package;

}


}
