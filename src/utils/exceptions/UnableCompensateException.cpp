#include "utils/exceptions/UnableCompensateException.hpp"

UnableCompensateException::UnableCompensateException() : GeneralException("Tidak dapat menebus properti.") {}
UnableCompensateException::UnableCompensateException(const std::string& msg) : GeneralException(msg) {}
