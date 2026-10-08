#ifndef HASHER_H
#define HASHER_H

#include <string>

// Thin wrapper around the SHA-256 library (picosha2).
// The rest of the project only ever calls Hasher, so the library can be swapped later.
class Hasher {
public:
    // Returns the 64-character lowercase hex SHA-256 digest of s.
    static string sha256(const std::string& s);
};

#endif
