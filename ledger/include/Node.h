#ifndef NODE_H
#define NODE_H

#include <string>
using namespace std;

#include "Blockchain.h"

// A simulated network participant: it has an id and its own private copy of the chain.
class Node {
public:
    Node(const string& id, int difficulty = 3);

    const string& getId() const { return id; }
    Blockchain& getChain() { return chain; }
    const Blockchain& getChain() const { return chain; }

    bool addTransaction(const Transaction& tx, string* error = nullptr);
    const Block* mine();

    // Consensus rule: adopt `other` only if it is longer AND valid.
    // Returns true if our chain was replaced.
    bool receiveChain(const Blockchain& other);

private:
    string id;
    Blockchain chain;
};

#endif
