#pragma once

#include <freertos/event_groups.h>

namespace ReadieFur::EspGps
{
    class Checkpoint //I forget what this is or why I have it.
    {
    public:
        enum EEventTrigger
        {
            GSM_FAILED = 1 << 0,
            GSM_OK = 1 << 1,
            MQTT_FAILED = 1 << 2,
            MQTT_OK = 1 << 3,
            PUBLISH_FAILED = 1 << 4,
            PUBLISH_OK = 1 << 5,
            FAILED = GSM_FAILED | MQTT_FAILED | PUBLISH_FAILED,
            OK = GSM_OK | MQTT_OK | PUBLISH_OK,
            ALL = FAILED | OK
        };

        static EventGroupHandle_t EventGroup;
    };
};

EventGroupHandle_t ReadieFur::EspGps::Checkpoint::EventGroup = xEventGroupCreate();
