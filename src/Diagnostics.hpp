#pragma once

#include "Service/AService.hpp"
#include <stdlib.h>
#include <string.h>
#include <map>
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "Helpers.h"
#include <freertos/FreeRTOSConfig.h>
#include <WString.h>

namespace ReadieFur::EspGps
{
    class Diagnostics : public Service::AService
    {
    private:
        #if configUSE_TRACE_FACILITY == 1
        static bool GetCpuTime(std::map<BaseType_t, uint32_t>* recordings)
        {
            // Get idle time for both CPU cores
            UBaseType_t arraySize = uxTaskGetNumberOfTasks();
            TaskStatus_t* tasksArray = (TaskStatus_t*)malloc(arraySize * sizeof(TaskStatus_t));

            if (tasksArray == nullptr)
                return false;

            arraySize = uxTaskGetSystemState(tasksArray, arraySize, NULL);
            for (int i = 0; i < arraySize; i++)
                if (strcmp(tasksArray[i].pcTaskName, "IDLE") == 0)
                    (*recordings)[tasksArray[i].xTaskNumber] = tasksArray[i].ulRunTimeCounter;

            free(tasksArray);

            return true;
        }
        #endif

        static void GetFreeMemory(size_t& outIram, size_t& outDram)
        {
            outIram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            outDram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        }

    protected:
        void RunServiceImpl() override
        {
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                #if configUSE_TRACE_FACILITY == 1
                std::map<BaseType_t, uint32_t> cpuRecordings;
                GetCpuTime(&cpuRecordings);
                String cpuLogString;
                for (auto &&recording : cpuRecordings)
                {
                    cpuLogString += "CPU";
                    cpuLogString += recording.first;
                    cpuLogString += ": ";
                    cpuLogString += recording.second;
                    cpuLogString += ", ";
                }
                if (cpuLogString.endsWith(", "))
                    cpuLogString = cpuLogString.substring(0, cpuLogString.length() - 2);
                cpuRecordings.clear();
                ESP_LOGD(nameof(Diagnostics), cpuLogString.c_str());
                cpuLogString.clear();
                #endif

                size_t iram, dram;
                GetFreeMemory(iram, dram);
                ESP_LOGD(nameof(Diagnostics), "Memory free: IRAM: %u, DRAM: %u", iram, dram);

                vTaskDelay(pdMS_TO_TICKS(5 * 1000));
            }
        }
    
    public:
        Diagnostics()
        {
            ServiceEntrypointStackDepth += 256;
        }
    };
};
