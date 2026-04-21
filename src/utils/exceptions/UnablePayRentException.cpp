#include "utils/exceptions/UnablePayRentException.hpp"

UnablePayRentException::UnablePayRentException() : GeneralException("You are unable to pay the rent!") {}

UnablePayRentException::UnablePayRentException(const std::string& msg) : GeneralException(msg) {}
