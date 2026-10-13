#include "Node.h"

Node::Node(const std::string& id, int difficulty) : id(id), chain(difficulty) {}

bool Node::addTransaction(const Transaction& tx, std::string* error) {
    return chain.addTransaction(tx, error);
}

const Block* Node::mine() {
    return chain.minePending();
}

bool Node::receiveChain(const Blockchain& other) {
    return chain.replaceChain(other);
}
