#include "utils/exceptions/NoPropertyException.hpp"

NoPropertyException::NoPropertyException() : GeneralException("You don't have any property!") {}

NoPropertyException::NoPropertyException(const std::string& msg) : GeneralException(msg) {}
