#include "utils/exceptions/UnablePayPropertyException.hpp"

UnablePayPropertyException::UnablePayPropertyException() : GeneralException("You are unable to pay property!") {}

UnablePayPropertyException::UnablePayPropertyException(const std::string& msg) : GeneralException(msg) {}
