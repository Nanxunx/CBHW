#pragma once

#include <string>
#include <vector>


namespace turtle335
{


struct ModelPackage
{

    // 模型名称
    std::string name;


    // 主 M2
    std::vector<std::string> m2Files;


    // Skin 文件列表
    std::vector<std::string> skins;


    // 外部动画文件
    std::vector<std::string> animations;


    // BLP纹理
    std::vector<std::string> textures;


    std::string PrimaryM2() const;


    bool IsValid() const
    {
        return
            !name.empty()
            &&
            !m2Files.empty();
    }

};


}