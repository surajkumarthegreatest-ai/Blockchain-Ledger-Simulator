#include "Node.h"

Node::Node(const std::string& id, const ChainConfig& config) : id(id), chain(config) {}

bool Node::addTransaction(const Transaction& tx, std::string* error) {
    return chain.addTransaction(tx, error);
}

const Block* Node::mine(const std::string& minerAddress) {
    return chain.minePending(minerAddress);
}

bool Node::receiveChain(const Blockchain& other) {
    return chain.replaceChain(other);
}
