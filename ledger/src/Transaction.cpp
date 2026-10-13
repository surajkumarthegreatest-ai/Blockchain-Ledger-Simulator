#include "Transaction.h"

#include <iomanip>
#include <sstream>

Transaction::Transaction(const std::string& sender, const std::string& receiver, double amount)
    : sender(sender), receiver(receiver), amount(amount) {}

std::string Transaction::toString() const {
    std::ostringstream out;
    out << sender << "->" << receiver << ":" << std::fixed << std::setprecision(2) << amount;
    return out.str();
}
