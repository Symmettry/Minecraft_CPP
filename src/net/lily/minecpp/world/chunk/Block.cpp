#include "Block.hpp"

std::map<uint16_t, BlockDescriptor> BlockUtil::blockData{};
std::map<uint16_t, std::vector<BlockModel>> BlockUtil::blockModels{};