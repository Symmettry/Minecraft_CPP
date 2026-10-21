#include "OnlineCrypto.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <memory>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

namespace OnlineCrypto {
bool createSharedSecret(std::vector<uint8_t>& secret, std::string& error) {
    secret.resize(16);
    if (RAND_bytes(secret.data(), static_cast<int>(secret.size())) == 1) return true;
    secret.clear();
    error = "Unable to generate encryption key";
    return false;
}

bool encryptRsa(const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& input,
                std::vector<uint8_t>& output, std::string& error) {
    const unsigned char* keyData = publicKey.data();
    const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        d2i_PUBKEY(nullptr, &keyData, static_cast<long>(publicKey.size())), EVP_PKEY_free);
    if (!key) {
        error = "Server sent an invalid RSA public key";
        return false;
    }
    const std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(
        EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
    size_t outputSize = 0;
    if (!context || EVP_PKEY_encrypt_init(context.get()) <= 0
        || EVP_PKEY_CTX_set_rsa_padding(context.get(), RSA_PKCS1_PADDING) <= 0
        || EVP_PKEY_encrypt(context.get(), nullptr, &outputSize, input.data(), input.size()) <= 0) {
        error = "Unable to initialize RSA encryption";
        return false;
    }
    output.resize(outputSize);
    if (EVP_PKEY_encrypt(context.get(), output.data(), &outputSize,
            input.data(), input.size()) <= 0) {
        output.clear();
        error = "Unable to encrypt login response";
        return false;
    }
    output.resize(outputSize);
    return true;
}

std::string serverHash(const std::string& serverId, const std::vector<uint8_t>& secret,
                       const std::vector<uint8_t>& publicKey) {
    SHA_CTX context{};
    std::array<uint8_t, SHA_DIGEST_LENGTH> digest{};
    SHA1_Init(&context);
    SHA1_Update(&context, serverId.data(), serverId.size());
    SHA1_Update(&context, secret.data(), secret.size());
    SHA1_Update(&context, publicKey.data(), publicKey.size());
    SHA1_Final(digest.data(), &context);

    const bool negative = (digest.front() & 0x80U) != 0;
    if (negative) {
        for (uint8_t& byte : digest) byte = static_cast<uint8_t>(~byte);
        for (auto iterator = digest.rbegin(); iterator != digest.rend(); ++iterator) {
            if (++*iterator != 0) break;
        }
    }
    size_t first = 0;
    while (first + 1 < digest.size() && digest[first] == 0) ++first;
    std::ostringstream result;
    if (negative) result << '-';
    result << std::hex << static_cast<int>(digest[first]);
    for (++first; first < digest.size(); ++first)
        result << std::setw(2) << std::setfill('0') << static_cast<int>(digest[first]);
    return result.str();
}
}
