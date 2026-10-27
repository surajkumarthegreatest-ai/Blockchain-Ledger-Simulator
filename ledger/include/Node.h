#ifndef NODE_H
#define NODE_H

#include <string>

#include "Blockchain.h"

// A simulated network participant: it has an id and its own private copy of the chain.
class Node {
public:
    explicit Node(const std::string& id, const ChainConfig& config = ChainConfig());

    const std::string& getId() const { return id; }
    Blockchain& getChain() { return chain; }
    const Blockchain& getChain() const { return chain; }

    bool addTransaction(const Transaction& tx, std::string* error = nullptr);

    // Mines the pending pool; the block reward goes to minerAddress.
    const Block* mine(const std::string& minerAddress);

    // Consensus rule: adopt `other` only if it follows the same rules, is valid, and has
    // more cumulative work. Returns true if our chain was replaced.
    bool receiveChain(const Blockchain& other);

private:
    std::string id;
    Blockchain chain;
};

#endif
