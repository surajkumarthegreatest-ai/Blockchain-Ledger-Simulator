#include <cassert>
#include <iostream>

#include "Block.h"

int main() {
    Block b(1, {Transaction("A", "B", 10)}, "0");
    std::string original = b.calculateHash();
    assert(b.hash == original);
    assert(original.size() == 64);

    // Changing any hashed field changes the hash.
    b.nonce = 7;
    assert(b.calculateHash() != original);
    b.nonce = 0;
    assert(b.calculateHash() == original);

    b.txs[0].amount = 11;
    assert(b.calculateHash() != original);
    b.txs[0].amount = 10;

    b.prevHash = "1";
    assert(b.calculateHash() != original);
    b.prevHash = "0";

    b.index = 2;
    assert(b.calculateHash() != original);
    b.index = 1;

    // Proof of work.
    b.mine(3);
    assert(b.hash.rfind("000", 0) == 0);
    assert(b.hash == b.calculateHash());
    assert(Block::meetsDifficulty(b.hash, 3));
    assert(!Block::meetsDifficulty("1abc", 1));

    std::cout << "test_block: OK\n";
    return 0;
}
