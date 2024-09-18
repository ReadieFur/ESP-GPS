#include <Arduino.h>
#include "Config.h"
#include "Boards.h"

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    // vTaskDelete(NULL);
    Serial.println("Loop");
    delay(1000);
}
