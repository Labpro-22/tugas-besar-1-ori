#include "utils/exceptions/UndesirableException.hpp"

UndesirableException::UndesirableException() : GeneralException("You encountered an undesirable event!") {}

UndesirableException::UndesirableException(const std::string& msg) : GeneralException(msg) {}
