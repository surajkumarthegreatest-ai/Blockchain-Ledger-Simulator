#ifndef BLOCK_H
#define BLOCK_H

#include <string>
#include <vector>

#include "Transaction.h"

// One node of the linked list. The Blockchain owns every Block (via raw `next` pointers).
class Block {
public:
    int index;
    long long timestamp;               // seconds since epoch
    std::vector<Transaction> txs;
    std::string prevHash;
    std::string hash;
    int nonce;
    Block* next;                       // nullptr for the tail

    Block(int index, const std::vector<Transaction>& txs, const std::string& prevHash);

    // index + timestamp + every transaction string + prevHash + nonce, concatenated.
    std::string serialize() const;

    // SHA-256 of serialize().
    std::string calculateHash() const;

    // Proof of Work: increment nonce until the hash starts with `difficulty` zeros.
    void mine(int difficulty);

    // True if `h` starts with `difficulty` zeros.
    static bool meetsDifficulty(const std::string& h, int difficulty);
};

#endif
