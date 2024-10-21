#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Board.h"
#include "Config.h"

void setup()
{
}

void loop()
{
    vTaskDelete(NULL);
}

#ifndef ARDUINO
extern "C" void app_main()
{
    setup();
    while (true)
        if (eTaskGetState(NULL) != eTaskState::eDeleted)
            loop();
}
#endif
