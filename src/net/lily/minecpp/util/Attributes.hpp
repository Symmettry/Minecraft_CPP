//
// Created by lily on 12/28/25.
//

#ifndef MINECRAFTCLIENT_ATTRIBUTES_HPP
#define MINECRAFTCLIENT_ATTRIBUTES_HPP
#include <cstdint>

#include "Math.hpp"

struct AttributeModifier {
    UUID uuid; // 128-bit UUID
    double amount;
    uint8_t operation;
};

#endif //MINECRAFTCLIENT_ATTRIBUTES_HPP