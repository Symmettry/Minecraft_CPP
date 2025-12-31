#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <tuple>
#include <sstream>

#include "BlockAtlas.hpp"
#include "lib/stb_image.h"
#include "lib/stb_image_write.h"

class BlockAtlas;

struct TextureTile {
    int width, height;
    std::vector<unsigned char> data;
};

constexpr int atlasTileSize = 16;
constexpr int atlasTilesPerRow = 20;

using BlockAtlasData = std::pair<std::vector<unsigned char>, BlockAtlas>;

class BlockAtlas {
public:
    std::unordered_map<std::string, std::pair<int,int>> blockAtlasPos;

    static TextureTile loadPNG(const std::string &path){
        int w, h, n;
        unsigned char* d = stbi_load(path.c_str(), &w, &h, &n, 4);
        if(!d) throw std::runtime_error("Failed to load " + path);
        TextureTile tile{w, h, std::vector<unsigned char>(d, d + w * h * 4)};
        stbi_image_free(d);
        return tile;
    }

    std::vector<unsigned char> createAtlas(const std::string &folder){
        std::vector<std::string> blockNames;
        return buildAtlas(folder, blockNames, false);
    }

    void saveAtlas(const std::string &folder){
        std::vector<std::string> blockNames;
        std::vector<unsigned char> atlas = buildAtlas(folder, blockNames, true);

        if(!stbi_write_png("assets/blockatlas.png", atlasTilesPerRow * atlasTileSize, atlasTilesPerRow * atlasTileSize, 4, atlas.data(), atlasTilesPerRow * atlasTileSize * 4))
            throw std::runtime_error("Failed to write block atlas PNG");

        std::ofstream datFile("assets/blockatlas.dat");
        if(!datFile.is_open()) throw std::runtime_error("Failed to write block atlas DAT");

        for(size_t j = 0; j < blockNames.size(); ++j){
            datFile << blockNames[j];
            if(j != blockNames.size() - 1) datFile << ',';
        }
    }

    static BlockAtlasData loadAtlas(const std::string &pngPath, const std::string &datPath){
        std::ifstream datFile(datPath);
        if(!datFile.is_open()) throw std::runtime_error("Failed to open blockatlas.dat");

        std::vector<std::string> blockNames;
        std::string line;
        std::getline(datFile, line);
        std::stringstream ss(line);
        std::string block;
        while(std::getline(ss, block, ',')) blockNames.push_back(block);
        datFile.close();

        int w, h, n;
        unsigned char* d = stbi_load(pngPath.c_str(), &w, &h, &n, 4);
        if(!d) throw std::runtime_error("Failed to load blockatlas.png");

        std::vector atlas(d, d + w * h * 4);
        stbi_image_free(d);

        BlockAtlas atlasObj;
        for(size_t i = 0; i < blockNames.size(); ++i){
            atlasObj.blockAtlasPos[blockNames[i]] = {static_cast<int>(i % atlasTilesPerRow), static_cast<int>(i / atlasTilesPerRow)};
        }

        return {atlas, atlasObj};
    }

    std::tuple<float,float,float,float> getBlockUV(const std::string &blockName) const {
        constexpr float tileSize = 1.0f / atlasTilesPerRow;
        const auto &[tx, ty] = blockAtlasPos.at(blockName);
        return {tx * tileSize, ty * tileSize, (tx + 1) * tileSize, (ty + 1) * tileSize};
    }

private:
    std::vector<unsigned char> buildAtlas(const std::string &folder, std::vector<std::string> &blockNames, bool fillBlockNames){
        std::vector<TextureTile> tiles;
        int i = 0;

        for(const auto &entry : std::filesystem::directory_iterator(folder)){
            if(entry.path().extension() != ".png" || !entry.is_regular_file()) continue;

            TextureTile tile = loadPNG(entry.path().string());
            tiles.push_back(tile);

            if(fillBlockNames) blockNames.push_back(entry.path().stem().string());
            blockAtlasPos[entry.path().stem().string()] = {i % atlasTilesPerRow, i / atlasTilesPerRow};
            ++i;
        }

        constexpr int atlasSize = atlasTilesPerRow * atlasTileSize;
        std::vector<unsigned char> atlas(atlasSize * atlasSize * 4, 0);

        for(size_t i1 = 0; i1 < tiles.size(); ++i1){
            const auto &tile = tiles[i1];
            const int tx = i1 % atlasTilesPerRow;
            const int ty = i1 / atlasTilesPerRow;
            for(int y = 0; y < atlasTileSize; ++y){
                for(int x = 0; x < atlasTileSize; ++x){
                    const int atlasIndex = 4 * ((ty * atlasTileSize + y) * atlasSize + (tx * atlasTileSize + x));
                    const int tileIndex = 4 * (y * atlasTileSize + x);
                    atlas[atlasIndex + 0] = tile.data[tileIndex + 0];
                    atlas[atlasIndex + 1] = tile.data[tileIndex + 1];
                    atlas[atlasIndex + 2] = tile.data[tileIndex + 2];
                    atlas[atlasIndex + 3] = tile.data[tileIndex + 3];
                }
            }
        }

        return atlas;
    }
};
