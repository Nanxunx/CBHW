#include "turtle335/model/ModelPackage.h"

namespace turtle335
{


std::string ModelPackage::PrimaryM2() const
{

    if(m2Files.empty())
    {
        return {};
    }



    for(const auto& file : m2Files)
    {

        if(
            file.size() >= 3 &&
            file.substr(
                file.size()-3
            ) == ".m2"
        )
        {
            return file;
        }

    }


    return m2Files.front();

}


}
