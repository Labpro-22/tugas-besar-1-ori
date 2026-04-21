#pragma once

#include "GeneralException.hpp"

class UnablePayPBMTaxException : public GeneralException
{
        public:
                UnablePayPBMTaxException();
                UnablePayPBMTaxException(const std::string& msg);
};
