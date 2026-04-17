#include "utils/exceptions/UnableCompensateException.hpp"

UnableCompensateException::UnableCompensateException() : GeneralException("You are unable to compensate!") {}

UnableCompensateException::UnableCompensateException(const std::string& msg) : GeneralException(msg) {}
