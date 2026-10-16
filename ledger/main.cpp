#include <chrono>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#include "Blockchain.h"
#include "Node.h"

// ---------- small input helpers ----------
static std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

static bool readInt(const std::string& prompt, int& out) {
    std::istringstream in(readLine(prompt));
    return static_cast<bool>(in >> out);
}

static bool readDouble(const std::string& prompt, double& out) {
    std::istringstream in(readLine(prompt));
    return static_cast<bool>(in >> out);
}

// ---------- menu actions ----------
static void doAddTransaction(Blockchain& chain) {
    std::string sender = readLine("Sender   : ");
    std::string receiver = readLine("Receiver : ");
    double amount;
    if (!readDouble("Amount   : ", amount)) {
        std::cout << "Invalid amount.\n";
        return;
    }
    std::string err;
    if (chain.addTransaction(Transaction(sender, receiver, amount), &err)) {
        std::cout << "Added to pending pool (" << chain.getPending().size() << " pending).\n";
    } else {
        std::cout << "Rejected: " << err << "\n";
    }
}

static void doMine(Blockchain& chain) {
    if (chain.getPending().empty()) {
        std::cout << "Nothing to mine: pending pool is empty.\n";
        return;
    }
    auto start = std::chrono::steady_clock::now();
    const Block* b = chain.minePending();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
    std::cout << "Mined block #" << b->index << "\n"
              << "  nonce : " << b->nonce << "\n"
              << "  hash  : " << b->hash << "\n"
              << "  time  : " << ms << " ms\n";
}

static void doValidate(const Blockchain& chain) {
    int bad = -1;
    std::string reason;
    if (chain.isValid(&bad, &reason)) {
        std::cout << "VALID\n";
    } else {
        std::cout << "INVALID at block " << bad << ": " << reason << "\n";
    }
}

static void doTamper(Blockchain& chain) {
    int idx;
    double amt;
    if (!readInt("Block index to tamper: ", idx) || !readDouble("New amount: ", amt)) {
        std::cout << "Invalid input.\n";
        return;
    }
    if (chain.tamper(idx, amt)) {
        std::cout << "Block " << idx << " tampered. Run Validate to see it detected.\n";
    } else {
        std::cout << "No block with index " << idx << ".\n";
    }
}

static void doBalance(const Blockchain& chain) {
    std::string name = readLine("Name: ");
    std::cout << name << " balance: " << chain.getBalance(name) << "\n";
}

// Three nodes: A mines, B and C sync. Then C's copy is tampered and must be rejected.
static void doNodeDemo(int difficulty) {
    std::cout << "\n=== Multi-node demo (difficulty " << difficulty << ") ===\n";
    Node a("A", difficulty), b("B", difficulty), c("C", difficulty);

    a.addTransaction(Transaction("alice", "bob", 25));
    a.addTransaction(Transaction("bob", "carol", 10));
    a.mine();
    a.addTransaction(Transaction("carol", "alice", 5));
    a.mine();
    std::cout << "A mined; A chain length = " << a.getChain().length() << "\n";

    std::cout << "B receives A's chain -> " << (b.receiveChain(a.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";
    std::cout << "C receives A's chain -> " << (c.receiveChain(a.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";

    // C is now tampered with, then tries to feed its chain to a fresh node D.
    c.getChain().tamper(1, 9999);
    Node d("D", difficulty);
    std::cout << "C tampered with block 1.\n";
    std::cout << "D receives C's (tampered) chain -> "
              << (d.receiveChain(c.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";
    std::cout << "D receives A's (honest) chain   -> "
              << (d.receiveChain(a.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";
    std::cout << "Final lengths: A=" << a.getChain().length() << " B=" << b.getChain().length()
              << " C=" << c.getChain().length() << " D=" << d.getChain().length() << "\n\n";
}

int main() {
    int difficulty = 3;
    {
        int d;
        if (readInt("Mining difficulty (1-5, default 3): ", d) && d >= 1 && d <= 5) {
            difficulty = d;
        }
    }
    Blockchain chain(difficulty);
    std::cout << "Ledger started with genesis block, difficulty " << difficulty << ".\n";

    while (true) {
        std::cout << "\n1) Add transaction\n"
                  << "2) Mine pending transactions\n"
                  << "3) Print chain\n"
                  << "4) Validate chain\n"
                  << "5) Tamper with a block (demo)\n"
                  << "6) Get balance\n"
                  << "7) Run multi-node demo\n"
                  << "0) Exit\n";
        int choice;
        if (!std::cin.good()) break;
        if (!readInt("> ", choice)) {
            if (std::cin.eof()) break;
            std::cout << "Please enter a number.\n";
            continue;
        }
        switch (choice) {
            case 1: doAddTransaction(chain); break;
            case 2: doMine(chain); break;
            case 3: chain.print(); break;
            case 4: doValidate(chain); break;
            case 5: doTamper(chain); break;
            case 6: doBalance(chain); break;
            case 7: doNodeDemo(difficulty); break;
            case 0: std::cout << "Bye.\n"; return 0;
            default: std::cout << "Unknown option.\n";
        }
    }
    return 0;
}
