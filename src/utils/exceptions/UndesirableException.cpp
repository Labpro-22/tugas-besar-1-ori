#include "utils/exceptions/UndesirableException.hpp"

UndesirableException::UndesirableException() : GeneralException("Kejadian tidak diinginkan.") {}
UndesirableException::UndesirableException(const std::string& msg) : GeneralException(msg) {}
