#include "include/models/log-entry/LogEntry.hpp"

LogEntry::LogEntry() : turn(0) {}

LogEntry::LogEntry(int turn, const std::string &username, const std::string &actionType, const std::string &description)
    : turn(turn), username(username), actionType(actionType), description(description) {}

std::string LogEntry::getLogEntry(){
    return "[Turn " + std::to_string(turn) + "] " + username + "| " + actionType + "| " + description + "\n";
}

int LogEntry::getTurn() const { return turn; }
std::string LogEntry::getUsername() const { return username; }
std::string LogEntry::getActionType() const { return actionType; }
std::string LogEntry::getDescription() const { return description; }
