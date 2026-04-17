#pragma once

#include <exception>
#include <string>

class GeneralException : public std::exception
{
        protected:
                std::string message;
        public:
                GeneralException();
                GeneralException(const std::string& msg);
                virtual ~GeneralException() = default;
                const char* what() const noexcept override;
};
