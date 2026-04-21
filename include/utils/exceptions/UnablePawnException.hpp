#pragma once

#include "GeneralException.hpp"

class UnablePawnException : public GeneralException
{
        public:
                UnablePawnException();
                UnablePawnException(const std::string& msg);
};

class UnablePawnExistBuildingException : public UnablePawnException
{
        public:
                UnablePawnExistBuildingException();
                UnablePawnExistBuildingException(const std::string& msg);
};

class UnablePawnNoPropertyException : public UnablePawnException
{
        public:
                UnablePawnNoPropertyException();
                UnablePawnNoPropertyException(const std::string& msg);
};
