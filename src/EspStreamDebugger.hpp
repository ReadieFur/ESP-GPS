#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <iostream>
#include <Stream.h>

class EspStreamDebugger
  : public Stream
{
  private:
    Stream& _data;
    TaskHandle_t _relayTaskHandle = nullptr;

    static void relayTask(void* args)
    {
        EspStreamDebugger* self = static_cast<EspStreamDebugger*>(args);

        while(true)
        {
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(0)) > 0 || self == nullptr)
            {
                vTaskDelete(NULL);
                return;
            }

            if (self->_data.available()) {
                std::cout << self->_data.read();
            }
            // if (_dump.available()) {
            //   _data.write(_dump.read());
            // }
            // vTaskDelay(0);
        }
    }

  public:
    EspStreamDebugger(Stream& data, bool autoStartRelayTask = false)
      : _data(data)
    {}

    ~EspStreamDebugger()
    {
        stopRelayTask();
    }

    virtual size_t write(uint8_t ch) {
      std::cout << ch;
      return _data.write(ch);
    }
    virtual int read() {
      int ch = _data.read();
      if (ch != -1) { std::cout << ch; }
      return ch;
    }
    virtual int available() { return _data.available(); }
    virtual int peek()      { return _data.peek();      }
    virtual void flush()    { _data.flush();            }

    void beginRelayTask()
    {
        if (_relayTaskHandle == nullptr)
            xTaskCreate(this->relayTask, "EspStreamDebuggerRelayTask", configMINIMAL_STACK_SIZE + 512, this, 2, &_relayTaskHandle);
    }

    void stopRelayTask()
    {
        if (_relayTaskHandle != nullptr)
        {
            xTaskNotifyGive(_relayTaskHandle);
            _relayTaskHandle = nullptr;
        }
    }
};
