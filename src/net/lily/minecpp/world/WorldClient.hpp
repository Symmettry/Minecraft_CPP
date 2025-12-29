//
// Created by lily on 12/28/25.
//

#ifndef MINECRAFTCLIENT_WORLDCLIENT_HPP
#define MINECRAFTCLIENT_WORLDCLIENT_HPP
#include "World.hpp"
#include "WorldSettings.hpp"

class NetHandlerPlay;

class WorldClient : public World {
    const NetHandlerPlay *sendQueue;

public:

    WorldClient(const NetHandlerPlay* netHandler, const WorldSettings settings, const int dimension, const EnumDifficulty::Value difficulty)
        : sendQueue(netHandler) {

        // todo
    }

};


#endif //MINECRAFTCLIENT_WORLDCLIENT_HPP