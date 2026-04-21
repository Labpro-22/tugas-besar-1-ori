#pragma once

#include "GeneralException.hpp"

class UnablePayBuildException : public GeneralException
{
        public:
                UnablePayBuildException();
                UnablePayBuildException(const std::string& msg);
};
