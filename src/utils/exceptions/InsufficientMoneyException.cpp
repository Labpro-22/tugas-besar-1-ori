#include "utils/exceptions/InsufficientMoneyException.hpp"

InsufficientMoneyException::InsufficientMoneyException() : GeneralException("You don't have sufficient money!") {}

InsufficientMoneyException::InsufficientMoneyException(const std::string& msg) : GeneralException(msg) {}
