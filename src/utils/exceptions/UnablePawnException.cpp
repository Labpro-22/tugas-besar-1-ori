#include "utils/exceptions/UnablePawnException.hpp"

UnablePawnException::UnablePawnException() : GeneralException("You are unable to pawn this property!") {}

UnablePawnException::UnablePawnException(const std::string& msg) : GeneralException(msg) {}

UnablePawnExistBuildingException::UnablePawnExistBuildingException() : UnablePawnException("You can't pawn a property that still has buildings on it!") {}

UnablePawnExistBuildingException::UnablePawnExistBuildingException(const std::string& msg) : UnablePawnException(msg) {}

UnablePawnNoPropertyException::UnablePawnNoPropertyException() : UnablePawnException("You don't have any property to pawn!") {}

UnablePawnNoPropertyException::UnablePawnNoPropertyException(const std::string& msg) : UnablePawnException(msg) {}
