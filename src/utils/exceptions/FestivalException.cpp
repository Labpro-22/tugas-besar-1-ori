#include "utils/exceptions/FestivalException.hpp"

FestivalException::FestivalException() : GeneralException("You are unable to hold a festival!") {}

FestivalException::FestivalException(const std::string& msg) : GeneralException(msg) {}

InvalidPropertyCodeException::InvalidPropertyCodeException() : FestivalException("You entered an invalid property code!") {}

InvalidPropertyCodeException::InvalidPropertyCodeException(const std::string& msg) : FestivalException(msg) {}

PropertyNotYoursException::PropertyNotYoursException() : FestivalException("You don't own this property!") {}

PropertyNotYoursException::PropertyNotYoursException(const std::string& msg) : FestivalException(msg) {}
