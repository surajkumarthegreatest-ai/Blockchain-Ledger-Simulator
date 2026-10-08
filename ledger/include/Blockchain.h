#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <string>
#include <vector>

#include "Block.h"
#include "Transaction.h"

// Singly linked list of Blocks plus a pool of pending transactions.
// Owns all its blocks: destructor frees them, copy operations deep-copy them.
class Blockchain {
public:
    explicit Blockchain(int difficulty = 3);
    Blockchain(const Blockchain& other);
    Blockchain& operator=(const Blockchain& other);
    ~Blockchain();

    // --- ledger behaviour ---
    // Validates (amount > 0, sender != receiver, non-empty names) and adds to the pending pool.
    // Returns false and fills `error` if rejected.
    bool addTransaction(const Transaction& tx, std::string* error = nullptr);

    // Packs the pending pool into a new block, mines it, appends it, clears the pool.
    // Returns the new block, or nullptr if the pool is empty.
    const Block* minePending();

    double getBalance(const std::string& name) const;

    // --- validation ---
    // Checks every block: hash matches content, hash meets difficulty,
    // prevHash matches the previous block, indexes are sequential.
    // On failure sets `badIndex` to the first bad block and `reason` to why.
    bool isValid(int* badIndex = nullptr, std::string* reason = nullptr) const;

    // --- demo only: corrupt a block's data so validation can catch it ---
    bool tamper(int index, double newAmount);

    // Replaces our blocks with other's if other is longer and valid. Keeps our pending pool.
    bool replaceChain(const Blockchain& other);

    // --- accessors ---
    int length() const { return size; }
    int getDifficulty() const { return difficulty; }
    const Block* getHead() const { return head; }
    const Block* getTail() const { return tail; }
    const Block* getBlock(int index) const;
    const std::vector<Transaction>& getPending() const { return pending; }

    void print() const;

private:
    Block* head;
    Block* tail;
    int size;
    int difficulty;
    std::vector<Transaction> pending;

    void appendBlock(Block* block);   // takes ownership
    void clearBlocks();
    void copyBlocksFrom(const Blockchain& other);
    Block* findBlock(int index);
};

#endif
