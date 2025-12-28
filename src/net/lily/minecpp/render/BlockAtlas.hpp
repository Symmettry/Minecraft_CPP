#pragma once

#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>
#include <tuple>
#include "lib/stb_image.h"

struct TextureTile {
    int width, height;
    std::vector<unsigned char> data;
};

constexpr int atlasTileSize = 16;
constexpr int atlasTilesPerRow = 20;

class BlockAtlas {
public:

    std::unordered_map<std::string, std::pair<int,int>> blockAtlasPos;

    static TextureTile loadPNG(const std::string &path){
        int w,h,n;
        unsigned char* d = stbi_load(path.c_str(), &w,&h,&n,4);
        if(!d) throw std::runtime_error("Failed to load "+path);
        TextureTile tile{w,h,std::vector<unsigned char>(d,d+w*h*4)};
        stbi_image_free(d);
        return tile;
    }

    std::vector<unsigned char> createAtlas(const std::string &folder){
        std::vector<TextureTile> tiles;
        int i=0;
        for(const auto &entry : std::filesystem::directory_iterator(folder)){
            if(entry.path().extension() != ".png" || !entry.is_regular_file()) continue;
            TextureTile tile = loadPNG(entry.path().string());
            tiles.push_back(tile);
            int tx = i % atlasTilesPerRow;
            int ty = i / atlasTilesPerRow;
            blockAtlasPos[entry.path().stem().string()] = {tx, ty};
            ++i;
        }

        constexpr int atlasSize = atlasTilesPerRow * atlasTileSize;
        std::vector<unsigned char> atlas(atlasSize * atlasSize * 4, 0);

        for(int i1=0; i1<tiles.size(); ++i1){
            const auto &tile = tiles[i1];
            const int tx = i1 % atlasTilesPerRow;
            const int ty = i1 / atlasTilesPerRow;
            for(int y=0; y<atlasTileSize; ++y){
                for(int x=0; x<atlasTileSize; ++x){
                    const int atlasIndex = 4*((ty*atlasTileSize + y)*atlasSize + (tx*atlasTileSize + x));
                    const int tileIndex = 4*(y*atlasTileSize + x);
                    atlas[atlasIndex + 0] = tile.data[tileIndex + 0];
                    atlas[atlasIndex + 1] = tile.data[tileIndex + 1];
                    atlas[atlasIndex + 2] = tile.data[tileIndex + 2];
                    atlas[atlasIndex + 3] = tile.data[tileIndex + 3];
                }
            }
        }
        return atlas;
    }

    std::tuple<float,float,float,float> getBlockUV(const std::string &blockName) const {
        constexpr float tileSize = 1.0f / atlasTilesPerRow;

        const auto&[fst, snd] = blockAtlasPos.at(blockName);
        const int tx = fst;
        const int ty = snd;

        const float uMin = tx * tileSize;
        const float vMin = ty * tileSize;
        const float uMax = uMin + tileSize;
        const float vMax = vMin + tileSize;
        return {uMin, vMin, uMax, vMax};
    }

};