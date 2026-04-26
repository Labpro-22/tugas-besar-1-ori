#include "include/models/log-entry/LogEntry.hpp"

LogEntry::LogEntry() : turn(0) {}

LogEntry::LogEntry(int turn, const std::string &username, const std::string &actionType, const std::string &description)
    : turn(turn), username(username), actionType(actionType), description(description) {}

std::string LogEntry::getLogEntry(){
    auto padRight = [](const std::string &s, int w) -> std::string {
        if ((int)s.size() >= w) return s.substr(0, w);
        return s + std::string(w - s.size(), ' ');
    };
    return "[Turn " + std::to_string(turn) + "] " +
           padRight(username, 10) + "| " +
           padRight(actionType, 8) + "| " +
           description;
}

int LogEntry::getTurn() const { return turn; }
std::string LogEntry::getUsername() const { return username; }
std::string LogEntry::getActionType() const { return actionType; }
std::string LogEntry::getDescription() const { return description; }
