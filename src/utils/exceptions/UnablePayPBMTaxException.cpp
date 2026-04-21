#include "utils/exceptions/UnablePayPBMTaxException.hpp"

UnablePayPBMTaxException::UnablePayPBMTaxException() : GeneralException("You are unable to pay the PBM tax!") {}

UnablePayPBMTaxException::UnablePayPBMTaxException(const std::string& msg) : GeneralException(msg) {}
