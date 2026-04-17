#include "utils/exceptions/GeneralException.hpp"

GeneralException::GeneralException() : message("") {}

GeneralException::GeneralException(const std::string& msg) : message(msg) {}

const char* GeneralException::what() const noexcept
{
        return message.c_str();
}
