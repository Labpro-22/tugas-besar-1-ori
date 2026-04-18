#include "utils/exceptions/SaveLoadException.hpp"

SaveLoadException::SaveLoadException() : GeneralException("Save/Load operation failed.") {}

SaveLoadException::SaveLoadException(const std::string &msg) : GeneralException(msg) {}
