#pragma once

#include "GeneralException.hpp"

class NoPropertyException : public GeneralException
{
        public:
                NoPropertyException();
                NoPropertyException(const std::string& msg);
};
