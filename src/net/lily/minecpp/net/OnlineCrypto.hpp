#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OnlineCrypto {
bool createSharedSecret(std::vector<uint8_t>& secret, std::string& error);
bool encryptRsa(const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& input,
                std::vector<uint8_t>& output, std::string& error);
std::string serverHash(const std::string& serverId, const std::vector<uint8_t>& secret,
                       const std::vector<uint8_t>& publicKey);
}
