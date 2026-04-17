#pragma once

#include "GeneralException.hpp"

class InsufficientMoneyException : public GeneralException
{
        public:
                InsufficientMoneyException();
                InsufficientMoneyException(const std::string& msg);
};
