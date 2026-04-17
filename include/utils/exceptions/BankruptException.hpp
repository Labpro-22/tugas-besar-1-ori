#pragma once

#include "GeneralException.hpp"

class BankruptException : public GeneralException
{
        public:
                BankruptException();
                BankruptException(const std::string& msg);
};

class BankruptCauseBankException : public BankruptException
{
        public:
                BankruptCauseBankException();
                BankruptCauseBankException(const std::string& msg);
};

class BankruptCausePlayerException : public BankruptException
{
        public:
                BankruptCausePlayerException();
                BankruptCausePlayerException(const std::string& msg);
};
