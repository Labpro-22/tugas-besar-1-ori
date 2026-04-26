#pragma once

#include "GeneralException.hpp"

class UndesirableException : public GeneralException
{
        public:
                UndesirableException();
                UndesirableException(const std::string& msg);
};
