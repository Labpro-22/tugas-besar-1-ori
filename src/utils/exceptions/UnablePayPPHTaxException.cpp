#include "utils/exceptions/UnablePayPPHTaxException.hpp"

UnablePayPPHTaxException::UnablePayPPHTaxException() : GeneralException("Tidak mampu membayar pajak PPH.") {}
UnablePayPPHTaxException::UnablePayPPHTaxException(const std::string& msg) : GeneralException(msg) {}
