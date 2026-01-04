#pragma once

#include "net/lily/minecpp/net/packets/ServerBoundPacket.hpp"
#include "net/lily/minecpp/net/packets/handshake/C00Handshake.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"

class C15PacketClientSettings final : public ServerBoundPacket {
public:
    const std::string langIn;
    const int viewIn, modelParts;
    const EnumChatVisibility::Value chatVisibility;
    const bool enableColors;

    explicit C15PacketClientSettings(const std::string& langIn, const int viewIn, const EnumChatVisibility::Value chatVisibility, const bool enableColors, const int modelParts)
        : ServerBoundPacket(0x15), langIn(langIn), viewIn(viewIn), modelParts(modelParts), chatVisibility(chatVisibility), enableColors(enableColors) {}

    [[nodiscard]] PacketBuffer serialize() const override {
        buf.writeString(langIn);
        buf.writeByte(viewIn);
        buf.writeByte(EnumChatVisibility::toId(chatVisibility));
        buf.writeBool(enableColors);
        buf.writeByte(modelParts);
        return buf;
    }
};
