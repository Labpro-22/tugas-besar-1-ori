#pragma once

#include "GeneralException.hpp"

class UnablePayPPHTaxException : public GeneralException
{
        public:
                UnablePayPPHTaxException();
                UnablePayPPHTaxException(const std::string& msg);
};
