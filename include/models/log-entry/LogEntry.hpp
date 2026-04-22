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
        LogEntry(int turn, const std::string &username, const std::string &actionType, const std::string &description);
        std::string getLogEntry();

        int getTurn() const;
        std::string getUsername() const;
        std::string getActionType() const;
        std::string getDescription() const;
};

#endif
