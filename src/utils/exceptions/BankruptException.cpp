#include "utils/exceptions/BankruptException.hpp"

BankruptException::BankruptException() : GeneralException("You are bankrupt!") {}

BankruptException::BankruptException(const std::string& msg) : GeneralException(msg) {}

BankruptCauseBankException::BankruptCauseBankException() : BankruptException("You are bankrupt because of the bank!") {}

BankruptCauseBankException::BankruptCauseBankException(const std::string& msg) : BankruptException(msg) {}

BankruptCausePlayerException::BankruptCausePlayerException() : BankruptException("You are bankrupt because of another player!") {}

BankruptCausePlayerException::BankruptCausePlayerException(const std::string& msg) : BankruptException(msg) {}
