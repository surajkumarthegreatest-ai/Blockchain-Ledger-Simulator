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
    int difficulty;                    // leading zero hex digits this block's hash must have
    std::vector<Transaction> txs;
    std::string prevHash;
    std::string hash;
    int nonce;
    Block* next;                       // nullptr for the tail

    Block(int index, const std::vector<Transaction>& txs, const std::string& prevHash,
          int difficulty);

    // Unambiguous text form of every hashed field. Fields are separated by '|' and each
    // transaction is length-prefixed, so different field values can never serialize equally.
    std::string serialize() const;

    // SHA-256 of serialize().
    std::string calculateHash() const;

    // Proof of Work: increment nonce until the hash starts with `difficulty` zeros.
    // Throws std::invalid_argument if difficulty is outside 0..64.
    void mine();

    // True if `h` starts with `difficulty` zeros.
    static bool meetsDifficulty(const std::string& h, int difficulty);
};

#endif
