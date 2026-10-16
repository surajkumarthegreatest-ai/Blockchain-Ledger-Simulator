#include <cassert>
#include <iostream>

#include "Blockchain.h"
#include "Node.h"

static Blockchain makeChain(int difficulty = 2) {
    Blockchain c(difficulty);
    c.addTransaction(Transaction("alice", "bob", 25));
    c.addTransaction(Transaction("bob", "carol", 10));
    c.minePending();
    c.addTransaction(Transaction("carol", "alice", 5));
    c.minePending();
    return c;
}

int main() {
    // Genesis + 2 mined blocks, links match, chain valid.
    {
        Blockchain c = makeChain();
        assert(c.length() == 3);
        assert(c.isValid());
        assert(c.getHead()->prevHash == "0");
        assert(c.getBlock(1)->prevHash == c.getBlock(0)->hash);
        assert(c.getBlock(2)->prevHash == c.getBlock(1)->hash);
        assert(c.getPending().empty());
    }
    // Transaction validation.
    {
        Blockchain c(1);
        std::string err;
        assert(!c.addTransaction(Transaction("a", "a", 5), &err));
        assert(!c.addTransaction(Transaction("a", "b", 0), &err));
        assert(!c.addTransaction(Transaction("a", "b", -3), &err));
        assert(!c.addTransaction(Transaction("", "b", 3), &err));
        assert(c.addTransaction(Transaction("a", "b", 3), &err));
        assert(c.minePending() != nullptr);
        assert(c.minePending() == nullptr);   // empty pool
    }
    // Balances.
    {
        Blockchain c = makeChain();
        assert(c.getBalance("alice") == -20.0);   // -25 + 5
        assert(c.getBalance("bob") == 15.0);      // +25 - 10
        assert(c.getBalance("carol") == 5.0);     // +10 - 5
    }
    // Tampered data is detected at the right block.
    {
        Blockchain c = makeChain();
        assert(c.tamper(1, 9999));
        int bad = -1;
        std::string reason;
        assert(!c.isValid(&bad, &reason));
        assert(bad == 1);
        assert(!reason.empty());
    }
    // Tampered hash (recomputed, so it is self-consistent but breaks PoW or the next link).
    {
        Blockchain c = makeChain();
        c.tamper(1, 9999);
        Block* b = const_cast<Block*>(c.getBlock(1));
        b->hash = b->calculateHash();
        int bad = -1;
        assert(!c.isValid(&bad));
        assert(bad == 1 || bad == 2);
    }
    // Broken link.
    {
        Blockchain c = makeChain();
        Block* b = const_cast<Block*>(c.getBlock(2));
        b->prevHash = "deadbeef";
        b->hash = b->calculateHash();
        int bad = -1;
        assert(!c.isValid(&bad));
        assert(bad == 2);
    }
    // Wrong difficulty (hash fails PoW check).
    {
        Blockchain c(1);
        Block* g = const_cast<Block*>(c.getHead());
        g->nonce = 0;
        do { ++g->nonce; g->hash = g->calculateHash(); } while (g->hash[0] == '0');
        int bad = -1;
        assert(!c.isValid(&bad));
        assert(bad == 0);
    }
    // Copy is deep: tampering the copy leaves the original valid.
    {
        Blockchain a = makeChain();
        Blockchain b = a;
        b.tamper(1, 1);
        assert(a.isValid());
        assert(!b.isValid());
    }
    // Nodes: longer valid chain accepted, tampered or shorter rejected.
    {
        Node a("A", 2), b("B", 2), c("C", 2);
        a.addTransaction(Transaction("x", "y", 1));
        a.mine();
        a.addTransaction(Transaction("y", "z", 1));
        a.mine();
        assert(b.receiveChain(a.getChain()));
        assert(b.getChain().length() == 3);
        assert(!b.receiveChain(a.getChain()));          // same length: not adopted

        a.getChain().tamper(1, 500);
        assert(!c.receiveChain(a.getChain()));          // longer but invalid
        assert(c.getChain().length() == 1);

        Node fresh("F", 2);
        assert(!a.receiveChain(fresh.getChain()));      // shorter: not adopted
    }
    std::cout << "test_chain: OK\n";
    return 0;
}
