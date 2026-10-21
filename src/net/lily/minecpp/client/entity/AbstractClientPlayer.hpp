//
// created by lily on 10/15/26
//

#ifndef MINECRAFTCLIENT_ABSTRACTCLIENTPLAYER_HPP
#define MINECRAFTCLIENT_ABSTRACTCLIENTPLAYER_HPP
#include "net/lily/minecpp/entity/player/EntityPlayer.hpp"


class AbstractClientPlayer : public EntityPlayer {

public:
    explicit AbstractClientPlayer(const Minecraft* mc, const float x, const float y, const float z) : EntityPlayer(mc, x, y, z) {}

};


#endif //mINECRAFTCLIENT_ABSTRACTCLIENTPLAYER_HPP
