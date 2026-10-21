#pragma once

#include <cstdint>
#include <variant>

struct SkullBlockEntity {
    uint8_t type = 0;
    uint8_t rotation = 0;
};

// add new block-entity payloads to this variant. Keeping only render/gameplay data avoids retaining entire NBT trees and makes reads allocation-free
using BlockEntityPayload = std::variant<SkullBlockEntity>;

struct BlockEntity {
    uint8_t updateAction = 0;
    BlockEntityPayload payload = SkullBlockEntity{};
};
