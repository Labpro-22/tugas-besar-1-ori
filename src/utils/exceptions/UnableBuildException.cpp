#include "utils/exceptions/UnableBuildException.hpp"

UnableBuildException::UnableBuildException() : GeneralException("You are unable to build!") {}

UnableBuildException::UnableBuildException(const std::string& msg) : GeneralException(msg) {}

InsufficientMonopolyTermsException::InsufficientMonopolyTermsException() : UnableBuildException("You don't fulfill the monopoly terms to build!") {}

InsufficientMonopolyTermsException::InsufficientMonopolyTermsException(const std::string& msg) : UnableBuildException(msg) {}

BuildingDifferenceException::BuildingDifferenceException() : UnableBuildException("You can't exceed the building difference limit within a monopoly!") {}

BuildingDifferenceException::BuildingDifferenceException(const std::string& msg) : UnableBuildException(msg) {}
