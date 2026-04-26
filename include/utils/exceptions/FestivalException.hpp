#pragma once

#include "GeneralException.hpp"

class FestivalException : public GeneralException
{
        public:
                FestivalException();
                FestivalException(const std::string& msg);
};

class InvalidPropertyCodeException : public FestivalException
{
        public:
                InvalidPropertyCodeException();
                InvalidPropertyCodeException(const std::string& msg);
};

class PropertyNotYoursException : public FestivalException
{
        public:
                PropertyNotYoursException();
                PropertyNotYoursException(const std::string& msg);
};
