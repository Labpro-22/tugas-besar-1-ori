#include "utils/exceptions/NoPropertyException.hpp"

NoPropertyException::NoPropertyException() : GeneralException("Tidak ada properti.") {}
NoPropertyException::NoPropertyException(const std::string& msg) : GeneralException(msg) {}
