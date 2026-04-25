#include "utils/exceptions/BankruptException.hpp"

BankruptException::BankruptException() : GeneralException("Pemain bangkrut.") {}
BankruptException::BankruptException(const std::string& msg) : GeneralException(msg) {}
BankruptCauseBankException::BankruptCauseBankException() : BankruptException("Bangkrut ke Bank.") {}
BankruptCauseBankException::BankruptCauseBankException(const std::string& msg) : BankruptException(msg) {}
BankruptCausePlayerException::BankruptCausePlayerException() : BankruptException("Bangkrut ke pemain lain.") {}
BankruptCausePlayerException::BankruptCausePlayerException(const std::string& msg) : BankruptException(msg) {}
