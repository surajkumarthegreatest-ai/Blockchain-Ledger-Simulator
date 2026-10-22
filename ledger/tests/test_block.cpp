#include <cassert>
#include <iostream>
#include <stdexcept>

#include "Block.h"

int main() {
    Block b(1, {Transaction("A", "B", 10, 1)}, "0", 2);
    const std::string original = b.calculateHash();
    assert(b.hash == original);
    assert(original.size() == 64);

    // Changing any hashed field changes the hash (and putting it back restores it).
    b.nonce = 7;
    assert(b.calculateHash() != original);
    b.nonce = 0;
    assert(b.calculateHash() == original);

    b.txs[0].amount = 11;
    assert(b.calculateHash() != original);
    b.txs[0].amount = 10;

    b.txs[0].signature = "sig";
    assert(b.calculateHash() != original);        // signatures are locked in by the hash
    b.txs[0].signature = "";

    b.prevHash = "1";
    assert(b.calculateHash() != original);
    b.prevHash = "0";

    b.index = 2;
    assert(b.calculateHash() != original);
    b.index = 1;

    b.difficulty = 3;
    assert(b.calculateHash() != original);
    b.difficulty = 2;

    b.timestamp += 1;
    assert(b.calculateHash() != original);
    b.timestamp -= 1;
    assert(b.calculateHash() == original);

    // Serialization is unambiguous: shifting a digit between neighbouring fields changes it.
    Block x(1, {}, "0", 2);
    x.timestamp = 23;
    Block y(12, {}, "0", 2);
    y.timestamp = 3;
    assert(x.serialize() != y.serialize());
    assert(x.calculateHash() != y.calculateHash());

    Block one(1, {Transaction("A", "B", 1, 1)}, "0", 2);
    Block two(1, {Transaction("A", "B", 1, 1), Transaction("A", "B", 1, 1)}, "0", 2);
    one.timestamp = two.timestamp = 5;
    assert(one.serialize() != two.serialize());

    // Proof of work uses the block's own difficulty.
    b.mine();
    assert(b.hash.rfind("00", 0) == 0);
    assert(b.hash == b.calculateHash());
    assert(Block::meetsDifficulty(b.hash, 2));
    assert(!Block::meetsDifficulty("1abc", 1));
    assert(Block::meetsDifficulty("abc", 0));
    assert(!Block::meetsDifficulty("00", 3));     // asking for more zeros than the hash has

    // Nonsense difficulties are refused instead of looping forever.
    bool threw = false;
    try {
        Block bad(1, {}, "0", 65);
        bad.mine();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    threw = false;
    try {
        Block bad(1, {}, "0", -1);
        bad.mine();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    std::cout << "test_block: OK\n";
    return 0;
}
