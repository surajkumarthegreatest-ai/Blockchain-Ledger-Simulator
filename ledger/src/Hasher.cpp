#include "Hasher.h"
#include "picosha2.h"

std::string Hasher::sha256(const std::string& s) {
    return picosha2::hash256_hex_string(s);
}
