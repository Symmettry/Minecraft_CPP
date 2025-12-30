#include "GameSettings.hpp"
#include "../Minecraft.hpp"
#include "net/lily/minecpp/net/packets/play/client/C15PacketClientSettings.hpp"

void GameSettings::sendSettingsToServer() const {
    if (!mc->player) return;

    int i = 0;
    for (int p=0;p<7;p++) {
        if (!modelParts[p]) continue;
        i |= EnumPlayerModelParts::getMask(p);
    }

    mc->netClient->sendPacket(C15PacketClientSettings{language, renderDistance, EnumChatVisibility::fromId(chatVisibility), chatColors, i});
}
