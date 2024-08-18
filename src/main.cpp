#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/task.h>

void setup()
{
}

#ifdef ARDUINO
void loop()
{
    vTaskDelete(NULL);
}
#else
extern "C" void app_main()
{
    setup();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
