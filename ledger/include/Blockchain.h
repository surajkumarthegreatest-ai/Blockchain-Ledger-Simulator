#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <string>
#include <unordered_map>
#include <vector>

#include "Block.h"
#include "Transaction.h"

// The rules of one chain. All nodes that want to share a chain must use identical rules.
struct ChainConfig {
    int initialDifficulty = 3;     // difficulty of the genesis block
    int targetBlockSeconds = 10;   // desired average time between blocks
    int adjustInterval = 5;        // difficulty is re-evaluated every N blocks (minimum 2)
    int minDifficulty = 1;
    int maxDifficulty = 5;         // keep this modest: each extra digit is ~16x more work
    long long reward = 50;         // coinbase amount paid to the miner of each block

    bool operator==(const ChainConfig& o) const {
        return initialDifficulty == o.initialDifficulty && targetBlockSeconds == o.targetBlockSeconds &&
               adjustInterval == o.adjustInterval && minDifficulty == o.minDifficulty &&
               maxDifficulty == o.maxDifficulty && reward == o.reward;
    }
};

// Running ledger state: who owns what, and the last nonce each sender used.
struct LedgerState {
    std::unordered_map<std::string, long long> balance;
    std::unordered_map<std::string, long long> lastNonce;

    long long balanceOf(const std::string& a) const {
        auto it = balance.find(a);
        return it == balance.end() ? 0 : it->second;
    }
    long long lastNonceOf(const std::string& a) const {
        auto it = lastNonce.find(a);
        return it == lastNonce.end() ? 0 : it->second;
    }
};

// Singly linked list of Blocks plus a pool of pending transactions.
// Owns all its blocks: destructor frees them, copy operations deep-copy them.
class Blockchain {
public:
    explicit Blockchain(const ChainConfig& config = ChainConfig());
    Blockchain(const Blockchain& other);
    Blockchain& operator=(const Blockchain& other);
    ~Blockchain();

    // ---- transactions ----
    // Accepts a transaction into the pending pool only if: names are sane, it is not a
    // coinbase, the signature is valid, the nonce is the next expected one for that sender,
    // and the sender can afford it (counting transactions already pending).
    bool addTransaction(const Transaction& tx, std::string* error = nullptr);

    // Builds a block = [coinbase reward to minerAddress] + every still-valid pending
    // transaction, mines it at the currently required difficulty, appends it and clears the
    // pool. Works with an empty pool (the block then holds only the reward).
    // Returns nullptr if minerAddress is empty or contains '|'.
    const Block* minePending(const std::string& minerAddress);

    // ---- ledger queries (all derived by walking the chain) ----
    long long getBalance(const std::string& address) const;        // confirmed balance
    long long availableBalance(const std::string& address) const;  // minus pending spends
    long long nextNonce(const std::string& address) const;         // nonce to use for the next tx

    // ---- validation ----
    // Replays the whole chain. Per block: sequential index, stored hash matches contents,
    // link to previous block, required difficulty, proof of work, timestamp sanity, and
    // ledger rules (one correct coinbase first, valid signatures, nonces in order, no overspend).
    // On failure sets `badIndex` to the first bad block and `reason` to why.
    bool isValid(int* badIndex = nullptr, std::string* reason = nullptr) const;

    // ---- difficulty ----
    // Pure function used by both mining and validation: given the blocks that come BEFORE a
    // block, what difficulty must that block have?
    static int computeExpectedDifficulty(const std::vector<const Block*>& prefix,
                                         const ChainConfig& config);
    int nextDifficulty() const;                 // difficulty the next mined block will need
    unsigned long long cumulativeWork() const;  // sum of 16^difficulty over all blocks

    // ---- consensus ----
    // Adopts `other` only if it uses the same rules, is valid, and represents MORE cumulative
    // work than our chain. Keeps our pending pool.
    bool replaceChain(const Blockchain& other);

    // ---- demo / test helpers (deliberately bypass the checks an honest node would do) ----
    // Corrupts the last transaction of a block (or adds one to an empty block) WITHOUT
    // recomputing the hash, so validation must catch it.
    bool tamper(int index, long long newAmount);
    // Appends a properly MINED block holding exactly `txs`, skipping every transaction rule.
    // Only the ledger rules in isValid() can catch what such a block does wrong.
    const Block* appendUnchecked(const std::vector<Transaction>& txs);

    // ---- accessors ----
    int length() const { return size; }
    const ChainConfig& getConfig() const { return cfg; }
    const Block* getHead() const { return head; }
    const Block* getTail() const { return tail; }
    const Block* getBlock(int index) const;
    const std::vector<Transaction>& getPending() const { return pending; }

    void print() const;

private:
    Block* head;
    Block* tail;
    int size;
    ChainConfig cfg;
    std::vector<Transaction> pending;
    mutable LedgerState confirmedState;
    mutable bool stateDirty;

    void ensureState() const;
    void rebuildState() const;

    void appendBlock(Block* block);   // takes ownership
    void clearBlocks();
    void copyBlocksFrom(const Blockchain& other);
    Block* findBlock(int index);
    std::vector<const Block*> blockPointers() const;
};

#endif