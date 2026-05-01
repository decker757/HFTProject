#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <openssl/hmac.h>
#include <openssl/evp.h>

std::string hmac_sha256(const std::string &key, const std::string &data)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    // The HMAC function
    // Parameters: Hash algorithm, Key, Key length, Data, Data length, Output buffer, Output length pointer
    unsigned char *result = HMAC(EVP_sha256(),
                                 key.c_str(), key.length(),
                                 reinterpret_cast<const unsigned char *>(data.c_str()), data.length(),
                                 hash, &len);

    if (result == nullptr)
        return "";

    // Convert to hex string for easy reading
    std::stringstream ss;
    for (unsigned int i = 0; i < len; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}