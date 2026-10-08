#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>

using namespace std;

class Transaction {
public:
    string sender;
    string receiver;
    double amount;

    Transaction(const string& sender, const string& receiver, double amount);

    // Fixed format, e.g. "A->B:10.00". Used when hashing, so it must be deterministic.
    string toString() const;
};

#endif
