#pragma once

#include "GeneralException.hpp"

class UnableCompensateException : public GeneralException
{
        public:
                UnableCompensateException();
                UnableCompensateException(const std::string& msg);
};
