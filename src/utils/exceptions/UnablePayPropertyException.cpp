#include "utils/exceptions/UnablePayPropertyException.hpp"

UnablePayPropertyException::UnablePayPropertyException() : GeneralException("Tidak mampu membayar properti.") {}
UnablePayPropertyException::UnablePayPropertyException(const std::string& msg) : GeneralException(msg) {}
