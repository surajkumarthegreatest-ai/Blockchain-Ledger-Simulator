#include <cassert>
#include <iostream>

#include "Hasher.h"

int main() {
    // Known SHA-256 test vector.
    assert(Hasher::sha256("abc") ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // Empty string vector.
    assert(Hasher::sha256("") ==
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    // Deterministic.
    assert(Hasher::sha256("abc") == Hasher::sha256("abc"));
    // Avalanche: different input, different hash.
    assert(Hasher::sha256("abc") != Hasher::sha256("abd"));
    // Fixed size.
    assert(Hasher::sha256("anything").size() == 64);
    std::cout << "test_hasher: OK\n";
    return 0;
}
