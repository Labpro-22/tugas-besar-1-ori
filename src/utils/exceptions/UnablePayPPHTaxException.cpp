#include "utils/exceptions/UnablePayPPHTaxException.hpp"

UnablePayPPHTaxException::UnablePayPPHTaxException() : GeneralException("You are unable to pay the PPH tax!") {}

UnablePayPPHTaxException::UnablePayPPHTaxException(const std::string& msg) : GeneralException(msg) {}
