#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <string>

#include "Blockchain.h"
#include "Wallet.h"

// A chain whose difficulty never changes during a test (adjustInterval is huge),
// so every block is mined at exactly `difficulty` and tests stay fast and deterministic.
inline ChainConfig stableConfig(int difficulty = 2) {
    ChainConfig cfg;
    cfg.initialDifficulty = difficulty;
    cfg.targetBlockSeconds = 10;
    cfg.adjustInterval = 1000;
    return cfg;
}

// A correctly signed payment from `from` to `to`, using the next unused nonce.
inline Transaction pay(const Blockchain& chain, const Wallet& from, const Wallet& to,
                       long long amount) {
    return from.createTransaction(to.address(), amount, chain.nextNonce(from.address()));
}

inline bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

#endif
