#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include "../../Packet.hpp"
#include "net/lily/minecpp/net/packets/ClientBoundPacket.hpp"
#include "ItemStack.hpp"

class S04PacketEntityEquipment : public ClientBoundPacket {
public:
    // todo
    // int32_t entityID = 0;
    // int16_t equipmentSlot = 0;
    // std::shared_ptr<ItemStack> itemStack;
    //
    // S04PacketEntityEquipment() : ClientBoundPacket(0x04) {}
    //
    // S04PacketEntityEquipment(int32_t entityIDIn, int16_t slot, const std::shared_ptr<ItemStack>& stack)
    //     : ClientBoundPacket(0x04), entityID(entityIDIn), equipmentSlot(slot),
    //       itemStack(stack ? std::make_shared<ItemStack>(*stack) : nullptr) {}
    //
    // static S04PacketEntityEquipment deserialize(const std::vector<uint8_t>& data) {
    //     S04PacketEntityEquipment packet;
    //     size_t offset = 0;
    //     packet.entityID = static_cast<int32_t>(readVarInt(data, offset));
    //
    //     if (offset + 2 > data.size()) throw std::runtime_error("Buffer too small for equipmentSlot");
    //     packet.equipmentSlot = static_cast<int16_t>((data[offset] << 8) | data[offset + 1]);
    //     offset += 2;
    //
    //     packet.itemStack = ItemStack::deserialize(data, offset);
    //     return packet;
    // }
};
