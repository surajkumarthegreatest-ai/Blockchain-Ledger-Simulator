#include <cassert>
#include <iostream>

#include "Transaction.h"

int main() {
    Transaction t("A", "B", 10);
    assert(t.toString() == "A->B:10.00");
    assert(t.toString() == t.toString());          // deterministic
    assert(Transaction("A", "B", 2.5).toString() == "A->B:2.50");
    assert(Transaction("A", "B", 10).toString() != Transaction("B", "A", 10).toString());
    std::cout << "test_transaction: OK\n";
    return 0;
}
