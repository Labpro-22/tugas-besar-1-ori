#include "include/models/log-entry/LogEntry.hpp"

LogEntry::LogEntry() {}

std::string LogEntry::getLogEntry(){
    return "[Turn " + std::to_string(turn) + "] " + username + "| " + actionType + "| " + description + "\n";
}