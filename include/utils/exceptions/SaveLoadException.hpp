#pragma once

#include "GeneralException.hpp"

class SaveLoadException : public GeneralException
{
public:
    SaveLoadException();
    SaveLoadException(const std::string &msg);
};
