#include "mod/MyMod.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mc/deps/core/math/Color.h"
#include "mc/nbt/CompoundTag.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockLegacy.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include "mod/stb_image.h"
#include "nlohmann/json.hpp"


#include "magic_enum/magic_enum.hpp"
#include "nlohmann/json_fwd.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <ll/api/memory/Hook.h>
#include <string>
#include <unordered_map>
#include <unordered_set>


nlohmann::json json;
nlohmann::json jblocks;
nlohmann::json jtextures;
nlohmann::json jnewcolormap;

static std::unordered_map<std::string, std::string> extra_map{
    {"sea_lantern",                 "seaLantern"              },
    {"trip_wire",                   "tripWire"                },
    {"sticky_piston_arm_collision", "stickyPistonArmCollision"},
    {"piston_arm_collision",        "pistonArmCollision"      },
    {"grass_block",                 "grass"                   },
    {"moving_block",                "movingBlock"             },
    {"invisible_bedrock",           "invisibleBedrock"        },
    {"dark_oak_planks",             "dark_oak_slab"           },
};
static std::unordered_set<std::string> eduList{
    "underwater_torch",
    "element_",
    "chalkboard",
    "material_reducer",
    "colored_torch_",
    "lab_table",
    "underwater_tnt",
    "compound_creator",
    "chemical_heat",
    "client_request_placeholder_block"
};

static std::unordered_map<std::string, std::unordered_set<std::string>> compatible_blocks{
    {"grass_block", {"grass"}},
};
std::string& replaceAll(std::string& a, const std::string& src, const std::string& dest) {
    std::string result = a;
    size_t      pos    = 0;
    while ((pos = result.find(src, pos)) != std::string::npos) {
        result.replace(pos, src.length(), dest);
        pos += dest.length();
    }
    a = result;
    return a;
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t                   start           = 0;
    size_t                   end             = str.find(delimiter);
    size_t                   delimiterLength = delimiter.length();

    while (end != std::string::npos) {
        std::string token = str.substr(start, end - start);
        if (!token.empty()) {
            tokens.push_back(token);
        }
        start = end + delimiterLength;
        end   = str.find(delimiter, start);
    }
    std::string lastToken = str.substr(start);
    if (!lastToken.empty()) {
        tokens.push_back(lastToken);
    }
    return tokens;
}

std::string_view getRenderLayerStr(enum class BlockRenderLayer layer) { return magic_enum::enum_name(layer); }
#define RES_DIR(a) (std::string("C:/Users/xhy/dev/bedrock-samples-main/resource_pack/") + a)
namespace my_mod {

MyMod& MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

bool MyMod::load() {
    getSelf().getLogger().debug("Loading...");
    // Code for loading the mod goes here.
    return true;
}

bool MyMod::enable() {
    std::ifstream(RES_DIR("blocks.json")) >> jblocks;
    std::ifstream(RES_DIR("textures/terrain_texture.json")) >> jtextures;
    BlockTypeRegistry::forEachBlock([&](const ::BlockLegacy& bl) {
        bl.forEachBlockPermutation([&](const ::Block& block) {
            auto           blockstr = ((CompoundTag&)(block.mSerializationId)).toSnbt(SnbtFormat::ForceQuote, 0);
            auto           str      = replaceAll(replaceAll(blockstr, ":1b", ":true"), ":0b", ":false");
            nlohmann::json bj;
            try {
                bj = nlohmann::json::parse(str);
            } catch (std::exception&) {
                printf("Can not Parse %s\n", str.c_str());
                return true;
            }
            bj["extra_data"]["render_layer"] = getRenderLayerStr(block.getLegacyBlock().mRenderLayer);
            auto textureName                 = block.getTypeName().substr(10);
            replaceAll(textureName, "_block_slab", "_slab");
            if (textureName.find("_glass") != std::string::npos) replaceAll(textureName, "hard_", "");
            replaceAll(textureName, "_double_slab", "_slab");
            if (!jblocks.contains(textureName)) {
                std::string newName;
                auto        vec = split(textureName, "_");
                if (vec.size() == 2 && vec[1] == "planks") {
                    newName = vec[0] + "_slab";
                }


                if (extra_map.contains(textureName)) {
                    newName = extra_map[textureName];
                }

                if (jblocks.contains(newName)) {
                    textureName = newName;
                } else {

                    if (!std::any_of(eduList.begin(), eduList.end(), [&](const std::string& edu) {
                            return textureName.find(edu) != std::string::npos;
                        })) {
                        printf("can not find texture name %s => %s\n", textureName.c_str(), newName.c_str());
                    }
                }
            }
            std::string upTex = "unknown";
            mce::Color  color;

            bj["extra_data"]["use_grass_color"] = std::unordered_set<std::string>{"grass"}.contains(textureName);

            bj["extra_data"]["use_leaves_color"] =
                std::unordered_set<std::string>{"leaves", "leaves2", "mangrove_leaves", "vine"}.contains(textureName);

            bj["extra_data"]["use_water_color"] =
                std::unordered_set<std::string>{"water", "flowing_water"}.contains(textureName);
            bj["extra_data"]["texture_name"] = textureName;

            bool doNotUseCarried = bj["extra_data"]["use_grass_color"] || bj["extra_data"]["use_leaves_color"];

            if (!jblocks.contains(textureName)) {
                //   printf("Duplicated texture: %s", textureName.c_str());
            } else {
                // logger.info(textureName);
                nlohmann::json textureMap;
                if (jblocks[textureName].contains("carried_textures") && !doNotUseCarried) {
                    textureMap = jblocks[textureName]["carried_textures"];
                } else if (jblocks[textureName].contains("textures")) {
                    textureMap = jblocks[textureName]["textures"];
                } else {
                    textureMap = "";
                }
                if (textureMap.is_string()) {
                    upTex = textureMap;
                } else {
                    upTex = textureMap["up"];
                }
                if (jtextures["texture_data"].contains(upTex)) {
                    auto texturePathMap = jtextures["texture_data"][upTex]["textures"];
                    if (texturePathMap.is_string()) {
                        upTex = texturePathMap;
                    } else {
                        // auto texturePath = texturePathMap[block. % texturePathMap.size()];
                        auto texturePath = texturePathMap[0];
                        if (texturePath.is_string()) {
                            upTex = texturePath;
                        } else {
                            upTex = texturePath["path"];
                        }
                    }
                } else {
                    upTex = "textures/blocks/" + upTex;
                }
                if (upTex != "") {
                    int            width, height, channel;
                    unsigned char* data = stbi_load(RES_DIR(upTex + ".png").c_str(), &width, &height, &channel, 4);
                    if (data == nullptr) {
                        data = stbi_load(RES_DIR(upTex + ".tga").c_str(), &width, &height, &channel, 4);
                    }
                    if (data != nullptr) {
                        float totalAlpha = 0;
                        for (int i = 0; i < width * height; i++) {
                            auto k4  = 4 * i;
                            color   += mce::Color(data[k4], data[k4 + 1], data[k4 + 2], data[k4 + 3]).sRGBToLinear()
                                   * (data[k4 + 3] / 255.0f);
                            totalAlpha += data[k4 + 3] / 255.0f;
                        }
                        color   /= totalAlpha;
                        color    = color.linearTosRGB();
                        color.a  = totalAlpha / (width * height);
                    } else {
                        if (block.getTypeName() != "minecraft:air") {
                            //   printf("%s", RES_DIR(upTex).c_str());
                        }
                        upTex = "unknown";
                    }
                }
            }
            bj["extra_data"]["textures"] = upTex;
            bj["extra_data"]["color"]    = {color.r, color.g, color.b, color.a};


            bj.erase("version");
            json.push_back(bj);
            auto name = bj["name"].get<std::string>();
            replaceAll(name, "minecraft:", "");
            auto it = compatible_blocks.find(name);
            if (it != compatible_blocks.end()) {
                printf("Find compatible block %s\n", name.c_str());
                auto& set = it->second;
                for (const auto& old : set) {
                    bj["name"] = "minecraft:" + old;
                    json.push_back(bj);
                }
            }
            return true;
        });
        return true;
    });
    std::ofstream("block_color.json") << json.dump(4);
    getSelf().getLogger().debug("Enabling...");
    // Code for enabling the mod goes here.
    return true;
}

bool MyMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    // Code for disabling the mod goes here.
    return true;
}

} // namespace my_mod

LL_REGISTER_MOD(my_mod::MyMod, my_mod::MyMod::getInstance());
