#include "Wallet.h"

Wallet::Wallet() : keys(Signer::generateKeyPair()), addr(Signer::addressFromPublicKey(keys.publicKey)) {}

Wallet::Wallet(const KeyPair& keys)
    : keys(keys), addr(Signer::addressFromPublicKey(keys.publicKey)) {}

std::string Wallet::sign(const std::string& message) const {
    return Signer::sign(keys.privateKey, message);
}

Transaction Wallet::createTransaction(const std::string& receiverAddress, long long amount,
                                      long long nonce) const {
    Transaction tx(addr, receiverAddress, amount, nonce, keys.publicKey, "");
    tx.signature = sign(tx.signingPayload());
    return tx;
}
