#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

enum class ProtocolState : uint8_t { Handshaking, Status, Login, Play };
enum class PacketDirection : uint8_t { Clientbound, Serverbound };

struct PacketDescriptor {
    uint8_t id;
    ProtocolState state;
    PacketDirection direction;
    std::string_view name;
};

inline constexpr std::array protocol47Packets{
    PacketDescriptor{0x00, ProtocolState::Handshaking, PacketDirection::Serverbound, "Handshake"},
    PacketDescriptor{0x00, ProtocolState::Status, PacketDirection::Serverbound, "Request"},
    PacketDescriptor{0x01, ProtocolState::Status, PacketDirection::Serverbound, "Ping"},
    PacketDescriptor{0x00, ProtocolState::Status, PacketDirection::Clientbound, "Response"},
    PacketDescriptor{0x01, ProtocolState::Status, PacketDirection::Clientbound, "Pong"},
    PacketDescriptor{0x00, ProtocolState::Login, PacketDirection::Serverbound, "Login Start"},
    PacketDescriptor{0x01, ProtocolState::Login, PacketDirection::Serverbound, "Encryption Response"},
    PacketDescriptor{0x00, ProtocolState::Login, PacketDirection::Clientbound, "Disconnect"},
    PacketDescriptor{0x01, ProtocolState::Login, PacketDirection::Clientbound, "Encryption Request"},
    PacketDescriptor{0x02, ProtocolState::Login, PacketDirection::Clientbound, "Login Success"},
    PacketDescriptor{0x03, ProtocolState::Login, PacketDirection::Clientbound, "Set Compression"},
#define PLAY_CLIENTBOUND_PACKET(id, name) PacketDescriptor{id, ProtocolState::Play, PacketDirection::Clientbound, name}
    PLAY_CLIENTBOUND_PACKET(0x00, "Keep Alive"), PLAY_CLIENTBOUND_PACKET(0x01, "Join Game"),
    PLAY_CLIENTBOUND_PACKET(0x02, "Chat Message"), PLAY_CLIENTBOUND_PACKET(0x03, "Time Update"),
    PLAY_CLIENTBOUND_PACKET(0x04, "Entity Equipment"), PLAY_CLIENTBOUND_PACKET(0x05, "Spawn Position"),
    PLAY_CLIENTBOUND_PACKET(0x06, "Update Health"), PLAY_CLIENTBOUND_PACKET(0x07, "Respawn"),
    PLAY_CLIENTBOUND_PACKET(0x08, "Player Position And Look"), PLAY_CLIENTBOUND_PACKET(0x09, "Held Item Change"),
    PLAY_CLIENTBOUND_PACKET(0x0A, "Use Bed"), PLAY_CLIENTBOUND_PACKET(0x0B, "Animation"),
    PLAY_CLIENTBOUND_PACKET(0x0C, "Spawn Player"), PLAY_CLIENTBOUND_PACKET(0x0D, "Collect Item"),
    PLAY_CLIENTBOUND_PACKET(0x0E, "Spawn Object"), PLAY_CLIENTBOUND_PACKET(0x0F, "Spawn Mob"),
    PLAY_CLIENTBOUND_PACKET(0x10, "Spawn Painting"), PLAY_CLIENTBOUND_PACKET(0x11, "Spawn Experience Orb"),
    PLAY_CLIENTBOUND_PACKET(0x12, "Entity Velocity"), PLAY_CLIENTBOUND_PACKET(0x13, "Destroy Entities"),
    PLAY_CLIENTBOUND_PACKET(0x14, "Entity"), PLAY_CLIENTBOUND_PACKET(0x15, "Entity Relative Move"),
    PLAY_CLIENTBOUND_PACKET(0x16, "Entity Look"), PLAY_CLIENTBOUND_PACKET(0x17, "Entity Look And Relative Move"),
    PLAY_CLIENTBOUND_PACKET(0x18, "Entity Teleport"), PLAY_CLIENTBOUND_PACKET(0x19, "Entity Head Look"),
    PLAY_CLIENTBOUND_PACKET(0x1A, "Entity Status"), PLAY_CLIENTBOUND_PACKET(0x1B, "Attach Entity"),
    PLAY_CLIENTBOUND_PACKET(0x1C, "Entity Metadata"), PLAY_CLIENTBOUND_PACKET(0x1D, "Entity Effect"),
    PLAY_CLIENTBOUND_PACKET(0x1E, "Remove Entity Effect"), PLAY_CLIENTBOUND_PACKET(0x1F, "Set Experience"),
    PLAY_CLIENTBOUND_PACKET(0x20, "Entity Properties"), PLAY_CLIENTBOUND_PACKET(0x21, "Chunk Data"),
    PLAY_CLIENTBOUND_PACKET(0x22, "Multi Block Change"), PLAY_CLIENTBOUND_PACKET(0x23, "Block Change"),
    PLAY_CLIENTBOUND_PACKET(0x24, "Block Action"), PLAY_CLIENTBOUND_PACKET(0x25, "Block Break Animation"),
    PLAY_CLIENTBOUND_PACKET(0x26, "Map Chunk Bulk"), PLAY_CLIENTBOUND_PACKET(0x27, "Explosion"),
    PLAY_CLIENTBOUND_PACKET(0x28, "Effect"), PLAY_CLIENTBOUND_PACKET(0x29, "Sound Effect"),
    PLAY_CLIENTBOUND_PACKET(0x2A, "Particle"), PLAY_CLIENTBOUND_PACKET(0x2B, "Change Game State"),
    PLAY_CLIENTBOUND_PACKET(0x2C, "Spawn Global Entity"), PLAY_CLIENTBOUND_PACKET(0x2D, "Open Window"),
    PLAY_CLIENTBOUND_PACKET(0x2E, "Close Window"), PLAY_CLIENTBOUND_PACKET(0x2F, "Set Slot"),
    PLAY_CLIENTBOUND_PACKET(0x30, "Window Items"), PLAY_CLIENTBOUND_PACKET(0x31, "Window Property"),
    PLAY_CLIENTBOUND_PACKET(0x32, "Confirm Transaction"), PLAY_CLIENTBOUND_PACKET(0x33, "Update Sign"),
    PLAY_CLIENTBOUND_PACKET(0x34, "Maps"), PLAY_CLIENTBOUND_PACKET(0x35, "Update Block Entity"),
    PLAY_CLIENTBOUND_PACKET(0x36, "Sign Editor Open"), PLAY_CLIENTBOUND_PACKET(0x37, "Statistics"),
    PLAY_CLIENTBOUND_PACKET(0x38, "Player List Item"), PLAY_CLIENTBOUND_PACKET(0x39, "Player Abilities"),
    PLAY_CLIENTBOUND_PACKET(0x3A, "Tab Complete"), PLAY_CLIENTBOUND_PACKET(0x3B, "Scoreboard Objective"),
    PLAY_CLIENTBOUND_PACKET(0x3C, "Update Score"), PLAY_CLIENTBOUND_PACKET(0x3D, "Display Scoreboard"),
    PLAY_CLIENTBOUND_PACKET(0x3E, "Teams"), PLAY_CLIENTBOUND_PACKET(0x3F, "Plugin Message"),
    PLAY_CLIENTBOUND_PACKET(0x40, "Disconnect"), PLAY_CLIENTBOUND_PACKET(0x41, "Server Difficulty"),
    PLAY_CLIENTBOUND_PACKET(0x42, "Combat Event"), PLAY_CLIENTBOUND_PACKET(0x43, "Camera"),
    PLAY_CLIENTBOUND_PACKET(0x44, "World Border"), PLAY_CLIENTBOUND_PACKET(0x45, "Title"),
    PLAY_CLIENTBOUND_PACKET(0x46, "Set Compression"), PLAY_CLIENTBOUND_PACKET(0x47, "Player List Header And Footer"),
    PLAY_CLIENTBOUND_PACKET(0x48, "Resource Pack Send"), PLAY_CLIENTBOUND_PACKET(0x49, "Update Entity NBT"),
#undef PLAY_CLIENTBOUND_PACKET
#define PLAY_SERVERBOUND_PACKET(id, name) PacketDescriptor{id, ProtocolState::Play, PacketDirection::Serverbound, name}
    PLAY_SERVERBOUND_PACKET(0x00, "Keep Alive"), PLAY_SERVERBOUND_PACKET(0x01, "Chat Message"),
    PLAY_SERVERBOUND_PACKET(0x02, "Use Entity"), PLAY_SERVERBOUND_PACKET(0x03, "Player"),
    PLAY_SERVERBOUND_PACKET(0x04, "Player Position"), PLAY_SERVERBOUND_PACKET(0x05, "Player Look"),
    PLAY_SERVERBOUND_PACKET(0x06, "Player Position And Look"), PLAY_SERVERBOUND_PACKET(0x07, "Player Digging"),
    PLAY_SERVERBOUND_PACKET(0x08, "Player Block Placement"), PLAY_SERVERBOUND_PACKET(0x09, "Held Item Change"),
    PLAY_SERVERBOUND_PACKET(0x0A, "Animation"), PLAY_SERVERBOUND_PACKET(0x0B, "Entity Action"),
    PLAY_SERVERBOUND_PACKET(0x0C, "Steer Vehicle"), PLAY_SERVERBOUND_PACKET(0x0D, "Close Window"),
    PLAY_SERVERBOUND_PACKET(0x0E, "Click Window"), PLAY_SERVERBOUND_PACKET(0x0F, "Confirm Transaction"),
    PLAY_SERVERBOUND_PACKET(0x10, "Creative Inventory Action"), PLAY_SERVERBOUND_PACKET(0x11, "Enchant Item"),
    PLAY_SERVERBOUND_PACKET(0x12, "Update Sign"), PLAY_SERVERBOUND_PACKET(0x13, "Player Abilities"),
    PLAY_SERVERBOUND_PACKET(0x14, "Tab Complete"), PLAY_SERVERBOUND_PACKET(0x15, "Client Settings"),
    PLAY_SERVERBOUND_PACKET(0x16, "Client Status"), PLAY_SERVERBOUND_PACKET(0x17, "Plugin Message"),
    PLAY_SERVERBOUND_PACKET(0x18, "Spectate"), PLAY_SERVERBOUND_PACKET(0x19, "Resource Pack Status")
#undef PLAY_SERVERBOUND_PACKET
};

constexpr std::optional<PacketDescriptor> findProtocol47Packet(
    const ProtocolState state, const PacketDirection direction, const uint8_t id) {
    for (const auto& packet : protocol47Packets) {
        if (packet.state == state && packet.direction == direction && packet.id == id) return packet;
    }
    return std::nullopt;
}
