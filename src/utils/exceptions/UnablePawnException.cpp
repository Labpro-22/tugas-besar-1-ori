#include "utils/exceptions/UnablePawnException.hpp"

UnablePawnException::UnablePawnException() : GeneralException("Tidak dapat menggadaikan properti.") {}
UnablePawnException::UnablePawnException(const std::string& msg) : GeneralException(msg) {}
UnablePawnExistBuildingException::UnablePawnExistBuildingException() : UnablePawnException("Masih ada bangunan di color group.") {}
UnablePawnExistBuildingException::UnablePawnExistBuildingException(const std::string& msg) : UnablePawnException(msg) {}
UnablePawnNoPropertyException::UnablePawnNoPropertyException() : UnablePawnException("Tidak ada properti yang bisa digadaikan.") {}
UnablePawnNoPropertyException::UnablePawnNoPropertyException(const std::string& msg) : UnablePawnException(msg) {}
