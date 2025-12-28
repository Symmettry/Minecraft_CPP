//
// Created by lily on 12/27/25.
//

#ifndef MINECRAFTCLIENT_C00PACKETLOGINSTART_HPP
#define MINECRAFTCLIENT_C00PACKETLOGINSTART_HPP
#include <utility>

class C00PacketLoginStart : public ServerBoundPacket {
public:
    std::string username;

    C00PacketLoginStart() : ServerBoundPacket(0x00) {}
    explicit C00PacketLoginStart(std::string  name) : ServerBoundPacket(0x00), username(std::move(name)) {}

    [[nodiscard]] std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer;
        writeString(username, buffer);
        return buffer;
    }
};


#endif //MINECRAFTCLIENT_C00PACKETLOGINSTART_HPP