#ifndef LOGENTRY_HPP
#define LOGENTRY_HPP

#include <string>

class LogEntry {
    private:
        int turn;
        std::string username;
        std::string actionType;
        std::string description;
    public: 
        LogEntry();
        std::string getLogEntry();
};

#endif