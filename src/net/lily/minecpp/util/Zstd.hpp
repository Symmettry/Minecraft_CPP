#pragma once

#include <fstream>
#include <zstd.h>
#include <stdexcept>
#include <string>
#include <vector>

class Zstd {
public:
    static std::string readZstdFile(const std::string &filename) {
        std::ifstream in(filename, std::ios::binary | std::ios::ate);
        if (!in.is_open()) throw std::runtime_error("Failed to open " + filename);
        const size_t compressedSize = in.tellg();
        in.seekg(0, std::ios::beg);

        std::vector<char> compressedData(compressedSize);
        in.read(compressedData.data(), compressedSize);
        in.close();

        unsigned long long const decompressedBound = ZSTD_getFrameContentSize(compressedData.data(), compressedSize);
        if (decompressedBound == ZSTD_CONTENTSIZE_ERROR) throw std::runtime_error("Not a valid zstd file");
        if (decompressedBound == ZSTD_CONTENTSIZE_UNKNOWN) throw std::runtime_error("Unknown decompressed size");

        std::vector<char> decompressedData(decompressedBound);
        if (size_t const dSize = ZSTD_decompress(decompressedData.data(), decompressedBound, compressedData.data(), compressedSize); ZSTD_isError(dSize)) throw std::runtime_error("ZSTD decompression failed: " + std::string(ZSTD_getErrorName(dSize)));

        return std::string(decompressedData.begin(), decompressedData.end());
    }
};
