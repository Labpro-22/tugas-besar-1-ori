#include "utils/exceptions/UnableBuildException.hpp"

UnableBuildException::UnableBuildException() : GeneralException("Tidak dapat membangun.") {}
UnableBuildException::UnableBuildException(const std::string& msg) : GeneralException(msg) {}
InsufficientMonopolyTermsException::InsufficientMonopolyTermsException() : UnableBuildException("Belum memonopoli color group.") {}
InsufficientMonopolyTermsException::InsufficientMonopolyTermsException(const std::string& msg) : UnableBuildException(msg) {}
BuildingDifferenceException::BuildingDifferenceException() : UnableBuildException("Bangunan tidak merata.") {}
BuildingDifferenceException::BuildingDifferenceException(const std::string& msg) : UnableBuildException(msg) {}
