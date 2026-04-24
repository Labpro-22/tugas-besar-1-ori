#include "utils/exceptions/UnablePayBuildException.hpp"

UnablePayBuildException::UnablePayBuildException() : GeneralException("Tidak mampu membayar pembangunan.") {}
UnablePayBuildException::UnablePayBuildException(const std::string& msg) : GeneralException(msg) {}
