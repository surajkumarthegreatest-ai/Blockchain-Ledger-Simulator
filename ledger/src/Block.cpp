#include "Block.h"

#include <chrono>
#include <sstream>

#include "Hasher.h"

Block::Block(int index, const std::vector<Transaction>& txs, const std::string& prevHash)
    : index(index),
      timestamp(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count()),
      txs(txs),
      prevHash(prevHash),
      hash(),
      nonce(0),
      next(nullptr) {
    hash = calculateHash();
}

std::string Block::serialize() const {
    std::ostringstream out;
    out << index << timestamp;
    for (const Transaction& tx : txs) {
        out << tx.toString();
    }
    out << prevHash << nonce;
    return out.str();
}

std::string Block::calculateHash() const {
    return Hasher::sha256(serialize());
}

bool Block::meetsDifficulty(const std::string& h, int difficulty) {
    if (difficulty < 0 || static_cast<size_t>(difficulty) > h.size()) {
        return false;
    }
    for (int i = 0; i < difficulty; ++i) {
        if (h[i] != '0') {
            return false;
        }
    }
    return true;
}

void Block::mine(int difficulty) {
    nonce = 0;
    hash = calculateHash();
    while (!meetsDifficulty(hash, difficulty)) {
        ++nonce;
        hash = calculateHash();
    }
}
