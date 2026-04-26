#pragma once

#include "GeneralException.hpp"

class SkillCardException : public GeneralException
{
        public:
                SkillCardException();
                SkillCardException(const std::string& msg);
};

class LimitOnlyOneException : public SkillCardException
{
        public:
                LimitOnlyOneException();
                LimitOnlyOneException(const std::string& msg);
};

class AfterDiceRollException : public SkillCardException
{
        public:
                AfterDiceRollException();
                AfterDiceRollException(const std::string& msg);
};
