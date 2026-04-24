#include "utils/exceptions/InsufficientMoneyException.hpp"

InsufficientMoneyException::InsufficientMoneyException() : GeneralException("Uang tidak cukup.") {}
InsufficientMoneyException::InsufficientMoneyException(const std::string& msg) : GeneralException(msg) {}
