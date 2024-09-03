#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(ESP8266)
#endif
#include "Config.h"

void Main()
{
}

#ifdef ARDUINO
void setup()
{
    Main();
}

void loop()
{
    //Shouldn't ever be reached.
    #ifdef ESP32
    vTaskDelete(NULL);
    #endif
}
#else
extern "C" void app_main()
{
    Main();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
