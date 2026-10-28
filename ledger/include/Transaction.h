#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>

class Transaction {
public:
    // Special sender used for the mining reward. It has no key and no signature.
    static const std::string COINBASE;

    std::string sender;        // address of the sender (= Signer::addressFromPublicKey(senderPubKey))
    std::string receiver;      // address of the receiver
    long long amount;          // whole coins (integers avoid floating-point rounding bugs)
    long long nonce;           // per-sender counter 1,2,3... stops a signed tx being replayed
    std::string senderPubKey;  // hex; empty for coinbase
    std::string signature;     // hex; empty for coinbase

    Transaction(const std::string& sender, const std::string& receiver, long long amount,
                long long nonce = 0, const std::string& senderPubKey = "",
                const std::string& signature = "");

    // Reward transaction paid to the miner of a block.
    static Transaction coinbase(const std::string& miner, long long reward);

    bool isCoinbase() const;

    // The exact text that gets signed: sender|receiver|amount|nonce  (no signature in it).
    std::string signingPayload() const;

    // Full text that goes into the block hash: payload|pubkey|signature.
    // Including the signature means the block hash also locks the signatures in.
    std::string toString() const;

    // Short human-readable form for printing (addresses are abbreviated).
    std::string shortString() const;

    // For normal transactions: the address matches the public key AND the signature is valid.
    // For coinbase: must carry no key and no signature.
    bool verifySignature() const;
};

#endif
