#include "utils/exceptions/UnablePayBuildException.hpp"

UnablePayBuildException::UnablePayBuildException() : GeneralException("You are unable to pay the build cost!") {}

UnablePayBuildException::UnablePayBuildException(const std::string& msg) : GeneralException(msg) {}
