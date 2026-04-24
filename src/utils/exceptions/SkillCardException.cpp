#include "utils/exceptions/SkillCardException.hpp"

SkillCardException::SkillCardException() : GeneralException("Kesalahan kartu kemampuan.") {}
SkillCardException::SkillCardException(const std::string& msg) : GeneralException(msg) {}
LimitOnlyOneException::LimitOnlyOneException() : SkillCardException("Hanya boleh menggunakan 1 kartu kemampuan per giliran.") {}
LimitOnlyOneException::LimitOnlyOneException(const std::string& msg) : SkillCardException(msg) {}
AfterDiceRollException::AfterDiceRollException() : SkillCardException("Kartu kemampuan hanya bisa digunakan sebelum melempar dadu.") {}
AfterDiceRollException::AfterDiceRollException(const std::string& msg) : SkillCardException(msg) {}
