#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "Blockchain.h"
#include "Node.h"
#include "Wallet.h"

namespace {

using WalletMap = std::map<std::string, Wallet>;

// ---------- small input helpers ----------
std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

bool readLongLong(const std::string& prompt, long long& out) {
    std::istringstream in(readLine(prompt));
    return static_cast<bool>(in >> out);
}

// Blank (or unreadable) input means "use the default".
int readIntDefault(const std::string& prompt, int def) {
    std::istringstream in(readLine(prompt));
    int v;
    return (in >> v) ? v : def;
}

std::string shortAddr(const std::string& a) {
    return a.size() > 12 ? a.substr(0, 10) + ".." : a;
}

const Wallet* findWallet(const WalletMap& wallets, const std::string& name) {
    auto it = wallets.find(name);
    if (it == wallets.end()) {
        std::cout << "No wallet named '" << name << "'. Create it first (option 1).\n";
        return nullptr;
    }
    return &it->second;
}

// ---------- menu actions ----------
void doCreateWallet(WalletMap& wallets) {
    std::string name = readLine("Wallet name: ");
    if (name.empty()) {
        std::cout << "Name must not be empty.\n";
        return;
    }
    if (wallets.count(name)) {
        std::cout << "A wallet called '" << name << "' already exists.\n";
        return;
    }
    auto it = wallets.emplace(name, Wallet()).first;
    std::cout << "Created wallet '" << name << "'\n  address: " << it->second.address() << "\n";
}

void doListWallets(const Blockchain& chain, const WalletMap& wallets) {
    if (wallets.empty()) {
        std::cout << "No wallets yet.\n";
        return;
    }
    for (const auto& entry : wallets) {
        const std::string& addr = entry.second.address();
        std::cout << "  " << entry.first << "  (" << shortAddr(addr) << ")  balance "
                  << chain.getBalance(addr) << "  available " << chain.availableBalance(addr)
                  << "\n";
    }
}

void doSend(Blockchain& chain, const WalletMap& wallets) {
    const Wallet* from = findWallet(wallets, readLine("From (wallet name): "));
    if (!from) return;
    const Wallet* to = findWallet(wallets, readLine("To   (wallet name): "));
    if (!to) return;
    long long amount;
    if (!readLongLong("Amount (whole coins): ", amount)) {
        std::cout << "Invalid amount.\n";
        return;
    }
    // The wallet signs with its private key; the chain only ever sees the signature.
    Transaction tx = from->createTransaction(to->address(), amount, chain.nextNonce(from->address()));
    std::string err;
    if (chain.addTransaction(tx, &err)) {
        std::cout << "Signed and added to the pending pool (" << chain.getPending().size()
                  << " pending).\n";
    } else {
        std::cout << "Rejected: " << err << "\n";
    }
}

void doMine(Blockchain& chain, const WalletMap& wallets) {
    const Wallet* miner = findWallet(wallets, readLine("Miner (wallet name, receives the reward): "));
    if (!miner) return;
    const int difficulty = chain.nextDifficulty();
    std::cout << "Mining at difficulty " << difficulty << "...\n";
    auto start = std::chrono::steady_clock::now();
    const Block* b = chain.minePending(miner->address());
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
    if (!b) {
        std::cout << "Could not mine (invalid miner address).\n";
        return;
    }
    std::cout << "Mined block #" << b->index << "\n"
              << "  difficulty : " << b->difficulty << "\n"
              << "  nonce      : " << b->nonce << "\n"
              << "  hash       : " << b->hash << "\n"
              << "  txs        : " << b->txs.size() << " (including the reward)\n"
              << "  time       : " << ms << " ms\n"
              << "  next block : difficulty " << chain.nextDifficulty() << "\n";
}

void doValidate(const Blockchain& chain) {
    int bad = -1;
    std::string reason;
    if (chain.isValid(&bad, &reason)) {
        std::cout << "VALID\n";
    } else {
        std::cout << "INVALID at block " << bad << ": " << reason << "\n";
    }
}

void doTamper(Blockchain& chain) {
    long long idx, amt;
    if (!readLongLong("Block index to tamper: ", idx) || !readLongLong("New amount: ", amt)) {
        std::cout << "Invalid input.\n";
        return;
    }
    if (chain.tamper(static_cast<int>(idx), amt)) {
        std::cout << "Block " << idx << " tampered. Run Validate to see it detected.\n";
    } else {
        std::cout << "No block with index " << idx << ".\n";
    }
}

// An attacker without the victim's key tries to spend the victim's coins. Runs on a COPY of
// the chain so the real ledger is not damaged.
void doForgeDemo(const Blockchain& chain, const WalletMap& wallets) {
    const Wallet* victim = findWallet(wallets, readLine("Victim (wallet name): "));
    if (!victim) return;
    Blockchain copy = chain;
    Wallet attacker;
    Transaction forged(victim->address(), attacker.address(), 1, copy.nextNonce(victim->address()),
                       attacker.publicKey(), "");
    forged.signature = attacker.sign(forged.signingPayload());

    std::string err;
    std::cout << "Attack 1: submit a payment 'from " << shortAddr(victim->address())
              << "' signed with the attacker's key\n";
    if (copy.addTransaction(forged, &err)) {
        std::cout << "  ACCEPTED (this should never happen!)\n";
    } else {
        std::cout << "  rejected by the pool: " << err << "\n";
    }

    std::cout << "Attack 2: skip the pool, mine the forged payment straight into a block\n";
    copy.appendUnchecked({Transaction::coinbase(attacker.address(), copy.getConfig().reward), forged});
    int bad = -1;
    std::string why;
    if (copy.isValid(&bad, &why)) {
        std::cout << "  VALID (this should never happen!)\n";
    } else {
        std::cout << "  INVALID at block " << bad << ": " << why << "\n";
    }
    std::cout << "(Your real chain was not touched.)\n";
}

void doInfo(const Blockchain& chain) {
    const ChainConfig& c = chain.getConfig();
    std::cout << "Blocks            : " << chain.length() << "\n"
              << "Next difficulty   : " << chain.nextDifficulty() << "\n"
              << "Cumulative work   : " << chain.cumulativeWork() << "\n"
              << "Block reward      : " << c.reward << "\n"
              << "Target block time : " << c.targetBlockSeconds << " s, re-evaluated every "
              << c.adjustInterval << " blocks (difficulty " << c.minDifficulty << ".."
              << c.maxDifficulty << ")\n"
              << "Pending txs       : " << chain.getPending().size() << "\n";
}

void printDifficulties(const Node& n) {
    std::cout << "  " << n.getId() << " difficulties:";
    for (const Block* b = n.getChain().getHead(); b != nullptr; b = b->next) {
        std::cout << " " << b->difficulty;
    }
    std::cout << "\n";
}

// Four nodes: A mines and pays, B and C sync, C is then tampered with and D must refuse it.
void doNodeDemo() {
    ChainConfig cfg;
    cfg.initialDifficulty = 2;
    cfg.maxDifficulty = 4;
    cfg.adjustInterval = 3;
    cfg.targetBlockSeconds = 5;
    std::cout << "\n=== Multi-node demo (difficulty starts at " << cfg.initialDifficulty
              << ", adjusts every " << cfg.adjustInterval << " blocks) ===\n";

    Wallet alice, bob, miner;
    Node a("A", cfg), b("B", cfg), c("C", cfg), d("D", cfg);

    a.mine(alice.address());   // alice earns the first reward
    a.addTransaction(alice.createTransaction(bob.address(), 20, a.getChain().nextNonce(alice.address())));
    a.mine(miner.address());
    a.mine(miner.address());   // blocks arrive instantly, so the difficulty rises
    std::cout << "A mined 3 blocks; chain length " << a.getChain().length() << "\n";
    printDifficulties(a);
    std::cout << "  alice=" << a.getChain().getBalance(alice.address())
              << " bob=" << a.getChain().getBalance(bob.address())
              << " miner=" << a.getChain().getBalance(miner.address()) << "\n";

    std::cout << "B receives A's chain -> " << (b.receiveChain(a.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";
    std::cout << "C receives A's chain -> " << (c.receiveChain(a.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";

    c.getChain().tamper(2, 9999);
    std::cout << "C's copy is tampered with (block 2).\n";
    std::cout << "D receives C's tampered chain -> " << (d.receiveChain(c.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";
    std::cout << "D receives A's honest chain   -> " << (d.receiveChain(a.getChain()) ? "ACCEPTED" : "REJECTED") << "\n";
    std::cout << "Final lengths: A=" << a.getChain().length() << " B=" << b.getChain().length()
              << " C=" << c.getChain().length() << " D=" << d.getChain().length() << "\n\n";
}

}  // namespace

int main() {
    ChainConfig cfg;
    cfg.initialDifficulty = readIntDefault("Initial difficulty (1-5, blank = 3): ", 3);
    cfg.targetBlockSeconds = readIntDefault("Target seconds per block (blank = 10): ", 10);
    cfg.adjustInterval = readIntDefault("Adjust difficulty every N blocks (blank = 5): ", 5);

    Blockchain chain(cfg);
    WalletMap wallets;
    const ChainConfig& used = chain.getConfig();
    std::cout << "Ledger started: genesis mined at difficulty " << used.initialDifficulty
              << ", reward " << used.reward << " per block.\n"
              << "Tip: create a wallet, mine a block to earn coins, then send some.\n";

    while (true) {
        std::cout << "\n 1) Create wallet\n"
                  << " 2) List wallets and balances\n"
                  << " 3) Send coins\n"
                  << " 4) Mine pending transactions\n"
                  << " 5) Print chain\n"
                  << " 6) Validate chain\n"
                  << " 7) Tamper with a block (demo)\n"
                  << " 8) Forged-signature attack demo\n"
                  << " 9) Run multi-node demo\n"
                  << "10) Chain info (difficulty, work, rules)\n"
                  << " 0) Exit\n";
        if (!std::cin.good()) break;
        long long choice;
        if (!readLongLong("> ", choice)) {
            if (std::cin.eof()) break;
            std::cout << "Please enter a number.\n";
            continue;
        }
        switch (choice) {
            case 1: doCreateWallet(wallets); break;
            case 2: doListWallets(chain, wallets); break;
            case 3: doSend(chain, wallets); break;
            case 4: doMine(chain, wallets); break;
            case 5: chain.print(); break;
            case 6: doValidate(chain); break;
            case 7: doTamper(chain); break;
            case 8: doForgeDemo(chain, wallets); break;
            case 9: doNodeDemo(); break;
            case 10: doInfo(chain); break;
            case 0: std::cout << "Bye.\n"; return 0;
            default: std::cout << "Unknown option.\n";
        }
    }
    return 0;
}
