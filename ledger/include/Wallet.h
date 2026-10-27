#ifndef WALLET_H
#define WALLET_H

#include <string>

#include "Signer.h"
#include "Transaction.h"

// A key pair plus the address derived from its public key.
// Whoever holds the private key is the only one who can spend from this address.
class Wallet {
public:
    Wallet();                           // generates a brand-new key pair
    explicit Wallet(const KeyPair& keys);

    const std::string& address() const { return addr; }
    const std::string& publicKey() const { return keys.publicKey; }

    // Signs an arbitrary message with this wallet's private key (hex DER signature).
    std::string sign(const std::string& message) const;

    // Builds a fully signed transaction from this wallet to `receiverAddress`.
    Transaction createTransaction(const std::string& receiverAddress, long long amount,
                                  long long nonce) const;

private:
    KeyPair keys;
    std::string addr;
};

#endif
