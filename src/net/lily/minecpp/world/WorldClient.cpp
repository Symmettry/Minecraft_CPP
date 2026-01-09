//
// Created by lily on 12/28/25.
//

#include "WorldClient.hpp"

#include "net/lily/minecpp/Minecraft.hpp"

WorldClient::WorldClient(const NetHandlerPlay* netHandler, const WorldSettings settings, const int dimension, const EnumDifficulty::Value difficulty)
        : World(Minecraft::getMinecraft()), sendQueue(netHandler) {

    // todo
}
