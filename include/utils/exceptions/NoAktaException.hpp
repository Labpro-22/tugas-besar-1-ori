#pragma once

#include "GeneralException.hpp"

class NoAktaException : public GeneralException
{
        public:
                NoAktaException();
                NoAktaException(const std::string& msg);
};
