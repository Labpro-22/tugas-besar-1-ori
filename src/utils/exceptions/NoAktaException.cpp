#include "utils/exceptions/NoAktaException.hpp"

NoAktaException::NoAktaException() : GeneralException("You don't have the required akta!") {}

NoAktaException::NoAktaException(const std::string& msg) : GeneralException(msg) {}
