#include "utils/exceptions/UnablePayRentException.hpp"

UnablePayRentException::UnablePayRentException() : GeneralException("Tidak mampu membayar sewa.") {}

UnablePayRentException::UnablePayRentException(const std::string& msg) : GeneralException(msg) {}
