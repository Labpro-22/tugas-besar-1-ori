#include "utils/exceptions/FestivalException.hpp"

FestivalException::FestivalException() : GeneralException("Kesalahan festival.") {}
FestivalException::FestivalException(const std::string& msg) : GeneralException(msg) {}
InvalidPropertyCodeException::InvalidPropertyCodeException() : FestivalException("Kode properti tidak valid.") {}
InvalidPropertyCodeException::InvalidPropertyCodeException(const std::string& msg) : FestivalException(msg) {}
PropertyNotYoursException::PropertyNotYoursException() : FestivalException("Properti bukan milik Anda.") {}
PropertyNotYoursException::PropertyNotYoursException(const std::string& msg) : FestivalException(msg) {}
