#include "utils/exceptions/SkillCardException.hpp"

SkillCardException::SkillCardException() : GeneralException("You are unable to use this skill card!") {}

SkillCardException::SkillCardException(const std::string& msg) : GeneralException(msg) {}

LimitOnlyOneException::LimitOnlyOneException() : SkillCardException("You can only use one skill card per turn!") {}

LimitOnlyOneException::LimitOnlyOneException(const std::string& msg) : SkillCardException(msg) {}

AfterDiceRollException::AfterDiceRollException() : SkillCardException("You can't use this skill card after rolling the dice!") {}

AfterDiceRollException::AfterDiceRollException(const std::string& msg) : SkillCardException(msg) {}
