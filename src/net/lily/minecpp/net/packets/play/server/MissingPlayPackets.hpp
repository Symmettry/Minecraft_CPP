#pragma once

#include "net/lily/minecpp/net/packets/OpaquePackets.hpp"

// these packets preserve the complete protocol-47 payload until their domain models (items, NBT, metadata, inventories, and scoreboards) are available
using S0APacketUseBed = OpaqueClientBoundPacket<0x0A>;
using S0BPacketAnimation = OpaqueClientBoundPacket<0x0B>;
using S0CPacketSpawnPlayer = OpaqueClientBoundPacket<0x0C>;
using S0DPacketCollectItem = OpaqueClientBoundPacket<0x0D>;
using S0EPacketSpawnObject = OpaqueClientBoundPacket<0x0E>;
using S10PacketSpawnPainting = OpaqueClientBoundPacket<0x10>;
using S11PacketSpawnExperienceOrb = OpaqueClientBoundPacket<0x11>;
using S12PacketEntityVelocity = OpaqueClientBoundPacket<0x12>;
using S13PacketDestroyEntities = OpaqueClientBoundPacket<0x13>;
using S14PacketEntity = OpaqueClientBoundPacket<0x14>;
using S15PacketEntityRelMove = OpaqueClientBoundPacket<0x15>;
using S16PacketEntityLook = OpaqueClientBoundPacket<0x16>;
using S17PacketEntityLookMove = OpaqueClientBoundPacket<0x17>;
using S18PacketEntityTeleport = OpaqueClientBoundPacket<0x18>;
using S19PacketEntityHeadLook = OpaqueClientBoundPacket<0x19>;
using S1APacketEntityStatus = OpaqueClientBoundPacket<0x1A>;
using S1BPacketEntityAttach = OpaqueClientBoundPacket<0x1B>;
using S1CPacketEntityMetadata = OpaqueClientBoundPacket<0x1C>;
using S1DPacketEntityEffect = OpaqueClientBoundPacket<0x1D>;
using S1EPacketRemoveEntityEffect = OpaqueClientBoundPacket<0x1E>;
using S27PacketExplosion = OpaqueClientBoundPacket<0x27>;
using S28PacketEffect = OpaqueClientBoundPacket<0x28>;
using S29PacketSoundEffect = OpaqueClientBoundPacket<0x29>;
using S2APacketParticles = OpaqueClientBoundPacket<0x2A>;
using S2BPacketChangeGameState = OpaqueClientBoundPacket<0x2B>;
using S2CPacketSpawnGlobalEntity = OpaqueClientBoundPacket<0x2C>;
using S2DPacketOpenWindow = OpaqueClientBoundPacket<0x2D>;
using S2EPacketCloseWindow = OpaqueClientBoundPacket<0x2E>;
using S2FPacketSetSlot = OpaqueClientBoundPacket<0x2F>;
using S30PacketWindowItems = OpaqueClientBoundPacket<0x30>;
using S31PacketWindowProperty = OpaqueClientBoundPacket<0x31>;
using S33PacketUpdateSign = OpaqueClientBoundPacket<0x33>;
using S34PacketMaps = OpaqueClientBoundPacket<0x34>;
using S36PacketSignEditorOpen = OpaqueClientBoundPacket<0x36>;
using S37PacketStatistics = OpaqueClientBoundPacket<0x37>;
using S38PacketPlayerListItem = OpaqueClientBoundPacket<0x38>;
using S39PacketPlayerAbilities = OpaqueClientBoundPacket<0x39>;
using S3APacketTabComplete = OpaqueClientBoundPacket<0x3A>;
using S3FPacketCustomPayload = OpaqueClientBoundPacket<0x3F>;
using S40PacketDisconnect = OpaqueClientBoundPacket<0x40>;
using S41PacketServerDifficulty = OpaqueClientBoundPacket<0x41>;
using S42PacketCombatEvent = OpaqueClientBoundPacket<0x42>;
using S43PacketCamera = OpaqueClientBoundPacket<0x43>;
using S44PacketWorldBorder = OpaqueClientBoundPacket<0x44>;
using S45PacketTitle = OpaqueClientBoundPacket<0x45>;
using S46PacketSetCompressionLevel = OpaqueClientBoundPacket<0x46>;
using S47PacketPlayerListHeaderFooter = OpaqueClientBoundPacket<0x47>;
using S48PacketResourcePackSend = OpaqueClientBoundPacket<0x48>;
using S49PacketUpdateEntityNBT = OpaqueClientBoundPacket<0x49>;
