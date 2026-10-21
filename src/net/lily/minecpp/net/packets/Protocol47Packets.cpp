#include "PacketCatalog.hpp"
#include "play/client/MissingPlayPackets.hpp"
#include "play/server/MissingPlayPackets.hpp"
#include "status/StatusPackets.hpp"

static_assert(protocol47Packets.size() == 111);

static_assert(findProtocol47Packet(
    ProtocolState::Play, PacketDirection::Clientbound, 0x49)->name == "Update Entity NBT");
static_assert(findProtocol47Packet(
    ProtocolState::Play, PacketDirection::Serverbound, 0x19)->name == "Resource Pack Status");
