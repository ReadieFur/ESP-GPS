//https://randomnerdtutorials.com/esp32-sim800l-publish-data-to-cloud/
//https://randomnerdtutorials.com/guide-to-neo-6m-gps-module-with-arduino/
//https://github.com/vshymanskyy/TinyGSM/blob/master/examples/HttpsClient/HttpsClient.ino
//https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/

#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/task.h>
#include "Config.h"
#include "Helpers.h"

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
    vTaskDelete(NULL);
}
#else
extern "C" void app_main()
{
    Main();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
