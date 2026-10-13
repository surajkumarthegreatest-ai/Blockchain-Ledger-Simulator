#include "Blockchain.h"

#include <iostream>
#include <unordered_map>

Blockchain::Blockchain(int difficulty)
    : head(nullptr), tail(nullptr), size(0), difficulty(difficulty) {
    // Genesis block: index 0, prevHash "0", mined like any other block.
    Block* genesis = new Block(0, {}, "0");
    genesis->mine(difficulty);
    appendBlock(genesis);
}

Blockchain::Blockchain(const Blockchain& other)
    : head(nullptr), tail(nullptr), size(0), difficulty(other.difficulty), pending(other.pending) {
    copyBlocksFrom(other);
}

Blockchain& Blockchain::operator=(const Blockchain& other) {
    if (this != &other) {
        clearBlocks();
        difficulty = other.difficulty;
        pending = other.pending;
        copyBlocksFrom(other);
    }
    return *this;
}

Blockchain::~Blockchain() {
    clearBlocks();
}

void Blockchain::appendBlock(Block* block) {
    if (tail == nullptr) {
        head = tail = block;
    } else {
        tail->next = block;
        tail = block;
    }
    ++size;
}

void Blockchain::clearBlocks() {
    Block* cur = head;
    while (cur != nullptr) {
        Block* nxt = cur->next;
        delete cur;
        cur = nxt;
    }
    head = tail = nullptr;
    size = 0;
}

void Blockchain::copyBlocksFrom(const Blockchain& other) {
    for (const Block* cur = other.head; cur != nullptr; cur = cur->next) {
        Block* copy = new Block(*cur);   // copies every field, including hash and nonce
        copy->next = nullptr;
        appendBlock(copy);
    }
}

bool Blockchain::addTransaction(const Transaction& tx, std::string* error) {
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };
    if (tx.sender.empty() || tx.receiver.empty()) return fail("sender and receiver must not be empty");
    if (tx.sender == tx.receiver) return fail("sender and receiver must differ");
    if (!(tx.amount > 0.0)) return fail("amount must be greater than 0");
    pending.push_back(tx);
    return true;
}

const Block* Blockchain::minePending() {
    if (pending.empty()) return nullptr;
    Block* block = new Block(tail->index + 1, pending, tail->hash);
    block->mine(difficulty);
    appendBlock(block);
    pending.clear();
    return block;
}

double Blockchain::getBalance(const std::string& name) const {
    double balance = 0.0;
    for (const Block* cur = head; cur != nullptr; cur = cur->next) {
        for (const Transaction& tx : cur->txs) {
            if (tx.sender == name) balance -= tx.amount;
            if (tx.receiver == name) balance += tx.amount;
        }
    }
    return balance;
}

bool Blockchain::isValid(int* badIndex, std::string* reason) const {
    auto fail = [&](int idx, const std::string& why) {
        if (badIndex) *badIndex = idx;
        if (reason) *reason = why;
        return false;
    };
    const Block* prev = nullptr;
    int expectedIndex = 0;
    for (const Block* cur = head; cur != nullptr; prev = cur, cur = cur->next, ++expectedIndex) {
        if (cur->index != expectedIndex) return fail(expectedIndex, "block index is out of sequence");
        if (cur->hash != cur->calculateHash()) return fail(cur->index, "stored hash does not match block contents");
        if (!Block::meetsDifficulty(cur->hash, difficulty)) return fail(cur->index, "hash does not meet proof-of-work difficulty");
        if (prev == nullptr) {
            if (cur->prevHash != "0") return fail(cur->index, "genesis prevHash must be \"0\"");
        } else if (cur->prevHash != prev->hash) {
            return fail(cur->index, "prevHash does not match previous block's hash");
        }
    }
    return true;
}

Block* Blockchain::findBlock(int index) {
    for (Block* cur = head; cur != nullptr; cur = cur->next) {
        if (cur->index == index) return cur;
    }
    return nullptr;
}

const Block* Blockchain::getBlock(int index) const {
    for (const Block* cur = head; cur != nullptr; cur = cur->next) {
        if (cur->index == index) return cur;
    }
    return nullptr;
}

bool Blockchain::tamper(int index, double newAmount) {
    Block* block = findBlock(index);
    if (block == nullptr) return false;
    if (block->txs.empty()) {
        block->txs.push_back(Transaction("Mallory", "Eve", newAmount));
    } else {
        block->txs[0].amount = newAmount;
    }
    // Deliberately does NOT recompute the hash: that is what validation must catch.
    return true;
}

bool Blockchain::replaceChain(const Blockchain& other) {
    if (other.size <= size) return false;
    if (!other.isValid()) return false;
    clearBlocks();
    difficulty = other.difficulty;
    copyBlocksFrom(other);
    return true;
}

void Blockchain::print() const {
    for (const Block* cur = head; cur != nullptr; cur = cur->next) {
        std::cout << "--------------------------------------------\n";
        std::cout << "Block #" << cur->index << "\n";
        std::cout << "  timestamp : " << cur->timestamp << "\n";
        std::cout << "  prevHash  : " << cur->prevHash << "\n";
        std::cout << "  hash      : " << cur->hash << "\n";
        std::cout << "  nonce     : " << cur->nonce << "\n";
        if (cur->txs.empty()) {
            std::cout << "  txs       : (none)\n";
        } else {
            std::cout << "  txs       :\n";
            for (const Transaction& tx : cur->txs) {
                std::cout << "    " << tx.toString() << "\n";
            }
        }
    }
    std::cout << "--------------------------------------------\n";
}
