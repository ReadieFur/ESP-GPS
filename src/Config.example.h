#pragma once

#define MODEM_APN               ""                  //The APN to be used by the module.
#define MODEM_PIN               ""                  //The pin to unlock the SIM card.
#define MODEM_USERNAME          ""                  //The username to be used to connect to the GSM network, leave blank for none.
#define MODEM_PASSWORD          ""                  //The password to be used to connect to the GSM network, leave blank for none.

#define MQTT_DEVICE_ID          ""                  //The device ID to use when sending to the broker.
#define MQTT_BROKER             ""                  //MQTT broker IP or domain.
#define MQTT_PORT               1883                //MQTT port.
#define MQTT_USERNAME           ""                  //MQTT Username, leave blank for none.
#define MQTT_PASSWORD           ""                  //MQTT Password, leave blank for none.
#define MQTT_TOPIC              ""                  //The MQTT topic base. Messages are published to MQTT_TOPIC/MQTT_CLIENT_ID/data, and the module will subscribe to MQTT_TOPIC/MQTT_CLIENT_ID/api
#define MQTT_CLIENT_ID          ""                  //The MQTT topic to subscribe to.

#define BATTERY_CRIT_SLEEP      10 * 60 * 1000      //Time in milliseconds to sleep when the battery is critically low.
#define BATTERY_LOW_INTERVAL    30 * 60 * 1000      //How often (in milliseconds) to publish data to the MQTT broker when the battery is low.
#define BATTERY_OK_INTERVAL     1 * 60 * 1000       //How often (in milliseconds) to publish data to the MQTT broker when the battery is above the low threshold.
#define BATTERY_CHRG_INTERVAL   5 * 1000            //How often (in milliseconds) to publish data to the MQTT broker when the battery is charging.

//Developer options.
#define DUMP_AT_COMMANDS
#define DUMP_GPS_COMMANDS
#define DUMP_GPS_DATA
#define DUMP_PUBLISH_DATA
