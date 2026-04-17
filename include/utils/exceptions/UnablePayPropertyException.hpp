#pragma once

#include "GeneralException.hpp"

class UnablePayPropertyException : public GeneralException
{
        public:
                UnablePayPropertyException();
                UnablePayPropertyException(const std::string& msg);
};
