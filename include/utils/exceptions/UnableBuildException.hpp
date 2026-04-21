#pragma once

#include "GeneralException.hpp"

class UnableBuildException : public GeneralException
{
        public:
                UnableBuildException();
                UnableBuildException(const std::string& msg);
};

class InsufficientMonopolyTermsException : public UnableBuildException
{
        public:
                InsufficientMonopolyTermsException();
                InsufficientMonopolyTermsException(const std::string& msg);
};

class BuildingDifferenceException : public UnableBuildException
{
        public:
                BuildingDifferenceException();
                BuildingDifferenceException(const std::string& msg);
};
