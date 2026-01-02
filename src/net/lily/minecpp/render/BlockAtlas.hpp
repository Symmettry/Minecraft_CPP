#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <tuple>
#include <sstream>
#include <string.h>

#include "BlockAtlas.hpp"
#include "lib/stb_image.h"
#include "lib/stb_image_write.h"
#include <zstd.h>

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
    std::unordered_map<const char*, std::pair<int,int>> blockAtlasPos;

    static TextureTile loadPNG(const std::string &path){
        int w, h, n;
        unsigned char* d = stbi_load(path.c_str(), &w, &h, &n, 4);
        if(!d) throw std::runtime_error("Failed to load " + path);
        TextureTile tile{w, h, std::vector(d, d + w * h * 4)};
        stbi_image_free(d);
        return tile;
    }

    std::vector<unsigned char> createAtlas(const std::string &folder){
        std::vector<std::string> blockNames;
        return buildAtlas(folder, blockNames, false);
    }

    // todo
    // void saveAtlas(const std::string &folder){
    //     std::vector<std::string> blockNames;
    //     std::vector<unsigned char> atlas = buildAtlas(folder, blockNames, true);
    //
    //     if(!stbi_write_png("assets/blockatlas.png", atlasTilesPerRow * atlasTileSize, atlasTilesPerRow * atlasTileSize, 4, atlas.data(), atlasTilesPerRow * atlasTileSize * 4))
    //         throw std::runtime_error("Failed to write block atlas PNG");
    //
    //     std::ofstream datFile("assets/blockatlas.dat.zst");
    //     if(!datFile.is_open()) throw std::runtime_error("Failed to write block atlas DAT");
    //
    //     for(size_t j = 0; j < blockNames.size(); ++j){
    //         datFile << blockNames[j];
    //         if(j != blockNames.size() - 1) datFile << ',';
    //     }
    // }

    static BlockAtlasData loadAtlas(const std::string &pngPath, const std::string &datPath){
        // Open .zst file
        std::ifstream datFile(datPath, std::ios::binary | std::ios::ate);
        if(!datFile.is_open()) throw std::runtime_error("Failed to open " + datPath);

        const size_t compressedSize = datFile.tellg();
        datFile.seekg(0, std::ios::beg);
        std::vector<char> compressedData(compressedSize);
        datFile.read(compressedData.data(), compressedSize);
        datFile.close();

        // Decompress
        unsigned long long const decompressedBound = ZSTD_getFrameContentSize(compressedData.data(), compressedSize);
        if(decompressedBound == ZSTD_CONTENTSIZE_ERROR) throw std::runtime_error("Not a valid zstd compressed file");
        if(decompressedBound == ZSTD_CONTENTSIZE_UNKNOWN) throw std::runtime_error("Unknown decompressed size");

        std::vector<char> decompressedData(decompressedBound);
        size_t const dSize = ZSTD_decompress(decompressedData.data(), decompressedBound, compressedData.data(), compressedSize);
        if(ZSTD_isError(dSize)) throw std::runtime_error("ZSTD decompression failed: " + std::string(ZSTD_getErrorName(dSize)));

        // Parse block names
        std::vector<std::string> blockNames;
        std::stringstream ss(std::string(decompressedData.begin(), decompressedData.end()));
        std::string block;
        while(std::getline(ss, block, ',')) blockNames.push_back(block);

        // Load PNG atlas
        int w, h, n;
        unsigned char* d = stbi_load(pngPath.c_str(), &w, &h, &n, 4);
        if(!d) throw std::runtime_error("Failed to load blockatlas.png");

        std::vector<unsigned char> atlas(d, d + w * h * 4);
        stbi_image_free(d);

        BlockAtlas atlasObj;
        for(size_t i = 0; i < blockNames.size(); ++i){
            atlasObj.blockAtlasPos[strdup(blockNames[i].c_str())] = {static_cast<int>(i % atlasTilesPerRow), static_cast<int>(i / atlasTilesPerRow)};
        }

        return {atlas, std::move(atlasObj)};
    }

    std::tuple<float,float,float,float> getBlockUV(const char* blockName) const {
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
            blockAtlasPos[strdup(entry.path().stem().string().c_str())] = {i % atlasTilesPerRow, i / atlasTilesPerRow};
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
