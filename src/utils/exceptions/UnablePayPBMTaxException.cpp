#include "utils/exceptions/UnablePayPBMTaxException.hpp"

UnablePayPBMTaxException::UnablePayPBMTaxException() : GeneralException("Tidak mampu membayar PBM.") {}
UnablePayPBMTaxException::UnablePayPBMTaxException(const std::string& msg) : GeneralException(msg) {}
