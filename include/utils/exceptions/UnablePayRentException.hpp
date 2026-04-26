#pragma once

#include "GeneralException.hpp"

class UnablePayRentException : public GeneralException
{
        public:
                UnablePayRentException();
                UnablePayRentException(const std::string& msg);
};
