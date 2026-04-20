#ifndef EVENTMANAGER_HPP
#define EVENTMANAGER_HPP

#include <string>
#include <vector>

class EventManager
{
private:
    std::vector<std::string> events;

public:
    void pushEvent(const std::string &event_text)
    {
        if (!event_text.empty())
        {
            events.push_back(event_text);
        }
    }

    bool hasEvents() const
    {
        return !events.empty();
    }

    std::vector<std::string> drainEvents()
    {
        std::vector<std::string> drained_events = events;
        events.clear();
        return drained_events;
    }
};

#endif
