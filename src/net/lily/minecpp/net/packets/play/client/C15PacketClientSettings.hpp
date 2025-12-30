#pragma once
#include <cstdint>
#include <vector>

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"

class C15PacketClientSettings : public ServerBoundPacket {
public:
    const std::string langIn;
    const int viewIn, modelParts;
    const EnumChatVisibility::Value chatVisibility;
    const bool enableColors;

    explicit C15PacketClientSettings(const std::string& langIn, const int viewIn, const EnumChatVisibility::Value chatVisibility, const bool enableColors, const int modelParts)
        : ServerBoundPacket(0x15), langIn(langIn), viewIn(viewIn), modelParts(modelParts), chatVisibility(chatVisibility), enableColors(enableColors) {}

    [[nodiscard]] std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeString(langIn, buffer);
        writeByte(viewIn, buffer);
        writeByte(EnumChatVisibility::toId(chatVisibility), buffer);
        writeBool(enableColors, buffer);
        writeByte(modelParts, buffer);
        return buffer;
    }
};
