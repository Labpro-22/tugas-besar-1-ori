#include "utils/exceptions/SaveLoadException.hpp"

SaveLoadException::SaveLoadException() : GeneralException("Gagal menyimpan atau memuat file.") {}
SaveLoadException::SaveLoadException(const std::string &msg) : GeneralException(msg) {}
