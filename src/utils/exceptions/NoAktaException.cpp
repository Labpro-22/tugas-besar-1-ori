#include "utils/exceptions/NoAktaException.hpp"

NoAktaException::NoAktaException() : GeneralException("Akta tidak ditemukan.") {}
NoAktaException::NoAktaException(const std::string& msg) : GeneralException(msg) {}
