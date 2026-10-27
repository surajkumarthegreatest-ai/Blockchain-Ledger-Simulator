#include "Block.h"

#include <chrono>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "Hasher.h"

Block::Block(int index, const std::vector<Transaction>& txs, const std::string& prevHash,
             int difficulty, long long ts)
    : index(index),
      timestamp(ts >= 0 ? ts : std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count()),
      difficulty(difficulty),
      txs(txs),
      prevHash(prevHash),
      hash(),
      nonce(0),
      next(nullptr) {
    hash = calculateHash();
}

std::string Block::serialize() const {
    std::ostringstream out;
    out << index << '|' << timestamp << '|' << difficulty << '|' << txs.size() << '|';
    for (const Transaction& tx : txs) {
        const std::string s = tx.toString();
        out << s.size() << ':' << s << '|';     // length prefix: no ambiguity between txs
    }
    out << prevHash << '|' << nonce;
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

void Block::mine() {
    if (difficulty < 0 || difficulty > 64) {
        throw std::invalid_argument("Block::mine: difficulty must be between 0 and 64");
    }
    nonce = 0;
    hash = calculateHash();
    while (!meetsDifficulty(hash, difficulty)) {
        if (nonce == std::numeric_limits<uint64_t>::max()) {
            // All 2^64 nonces exhausted without a solution (only possible at extreme
            // difficulty). Re-stamp the block to the current second, which changes
            // the search space, then restart from nonce 0 — exactly what Bitcoin does.
            timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
            nonce = 0;
        } else {
            ++nonce;
        }
        hash = calculateHash();
    }
}