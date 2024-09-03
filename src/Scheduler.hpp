#pragma once

#include <Arduino.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#elif defined(ESP8266)
#include <Ticker.h>
#endif
#include <map>
#include <string.h>

#if defined(ESP8266)
#define configMINIMAL_STACK_SIZE 0
#define configMAX_PRIORITIES 1
#endif

class Scheduler
{
private:
    typedef void (*TCallback)(void*);

    struct STask
    {
        #if defined(ESP32)
        TaskHandle_t handle;
        bool deleteTask;
        ulong interval;
        #elif defined (ESP8266)
        Ticker ticker;
        #endif

        TCallback callback;
        void* args;
    };

    #if defined(ESP32)
    static std::mutex mutex;
    #endif
    static std::map<ulong, STask*> tasks;

    static void CallbackWrapper(void* params)
    {
        STask* task = static_cast<STask*>(params);

        #if defined(ESP32)
        while (true)
        {
            if (task->deleteTask)
            {
                vTaskDelete(task->handle);
                return;
            }

            task->callback(task->args);
            vTaskDelay(task->interval);
        }
        #elif defined (ESP8266)
        task->callback(task->args);
        #endif
    }

    static void Lock()
    {
        #if defined(ESP32)
        mutex.lock();
        #endif
        noInterrupts();
    }

    static void Unlock()
    {
        #if defined(ESP32)
        mutex.unlock();
        #endif
        interrupts();
    }

public:
    static ulong Add(ulong interval, TCallback callback, void* args = nullptr, uint32_t esp32StackDepth = configMINIMAL_STACK_SIZE + 1024 * 4, uint esp32Priority = configMAX_PRIORITIES / 2)
    {
        Lock();

        //For safety I should have a proper mutex here (for the ESP8266) as interrupts could bypass this and cause duplicate values, although extremely unlikely.
        //See: https://github.com/raburton/esp8266/blob/master/mutex/mutex.c
        ulong taskId;
        do { taskId = millis(); }
        while (taskId == 0 || tasks.find(taskId) != tasks.end());

        STask* task = new STask{};
        task->callback = callback;
        task->args = args;

        #if defined(ESP32)
        task->deleteTask = false;
        task->interval = interval;
        xTaskCreate(CallbackWrapper, String(millis()).c_str(), esp32StackDepth, static_cast<void*>(task), esp32Priority, &task->handle);
        #elif defined(ESP8266)
        task->ticker.attach_ms(interval, CallbackWrapper, static_cast<void*>(task));
        #endif

        tasks.insert({ taskId, task });
        Unlock();
        return taskId;
    }

    static void Remove(ulong taskId)
    {
        Lock();

        auto task = tasks.find(taskId);
        if (task == tasks.end())
        {
            Unlock();
            return;
        }

        #if defined(ESP32)
        task->second->deleteTask = true;
        #elif defined(ESP8266)
        task->second->ticker.detach();
        #endif

        delete task->second;
        tasks.erase(taskId);

        Unlock();
    }
};

#if defined(ESP32)
std::mutex Scheduler::mutex;
#endif
std::map<ulong, Scheduler::STask*> Scheduler::tasks;

#if defined(ESP8266)
#undef configMINIMAL_STACK_SIZE
#undef configMAX_PRIORITIES
#endif
