#pragma once

#include "net/lily/minecpp/net/packets/OpaquePackets.hpp"

// raw-payload constructors allow all protocol-47 requests to be emitted now; domain-specific typed facades can replace these aliases without changing IDs
using C11PacketEnchantItem = OpaqueServerBoundPacket<0x11>;
using C12PacketUpdateSign = OpaqueServerBoundPacket<0x12>;
using C14PacketTabComplete = OpaqueServerBoundPacket<0x14>;
using C16PacketClientStatus = OpaqueServerBoundPacket<0x16>;
using C18PacketSpectate = OpaqueServerBoundPacket<0x18>;
using C19PacketResourcePackStatus = OpaqueServerBoundPacket<0x19>;
