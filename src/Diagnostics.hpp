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
        static bool GetCpuTime(std::map<BaseType_t, uint32_t>& outRecordings)
        {
            #if configUSE_TRACE_FACILITY == 1
            //Get idle time for all CPU cores.
            UBaseType_t arraySize = uxTaskGetNumberOfTasks();
            TaskStatus_t* tasksArray = (TaskStatus_t*)malloc(arraySize * sizeof(TaskStatus_t));

            if (tasksArray == nullptr)
                return false;

            arraySize = uxTaskGetSystemState(tasksArray, arraySize, NULL);
            for (int i = 0; i < arraySize; i++)
                if (strcmp(tasksArray[i].pcTaskName, "IDLE") == 0)
                    outRecordings[tasksArray[i].xTaskNumber] = tasksArray[i].ulRunTimeCounter;

            free(tasksArray);
            return true;
            #else
            return false;
            #endif
        }

        static void GetFreeMemory(size_t& outIram, size_t& outDram)
        {
            outIram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            outDram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        }

        static bool GetTasksFreeStack(std::map<const char*, size_t>& outRecordings)
        {
            #if configUSE_TRACE_FACILITY == 1
            UBaseType_t arraySize = uxTaskGetNumberOfTasks();
            TaskStatus_t* tasksArray = (TaskStatus_t*)malloc(arraySize * sizeof(TaskStatus_t));

            if (tasksArray == nullptr)
                return false;

            //Get system state (task status).
            UBaseType_t totalTasks = uxTaskGetSystemState(tasksArray, arraySize, NULL);

            //Print free stack space for each task.
            for (UBaseType_t i = 0; i < totalTasks; i++)
                outRecordings[tasksArray[i].pcTaskName] = tasksArray[i].usStackHighWaterMark * sizeof(StackType_t);

            //Free allocated memory for taskStatusArray.
            free(tasksArray);
            #else
            return false;
            #endif
        }

    protected:
        void RunServiceImpl() override
        {
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                // std::map<BaseType_t, uint32_t> cpuRecordings;
                // if (!GetCpuTime(cpuRecordings))
                // {
                //     String cpuLogString;
                //     for (auto &&recording : cpuRecordings)
                //     {
                //         cpuLogString += "CPU";
                //         cpuLogString += recording.first;
                //         cpuLogString += ": ";
                //         cpuLogString += recording.second;
                //         cpuLogString += ", ";
                //     }
                //     if (cpuLogString.endsWith(", "))
                //         cpuLogString = cpuLogString.substring(0, cpuLogString.length() - 2);
                //     cpuRecordings.clear();
                //     ESP_LOGD(nameof(Diagnostics), "%s", cpuLogString.c_str());
                //     cpuLogString.clear();
                // }

                size_t iram, dram;
                GetFreeMemory(iram, dram);
                ESP_LOGD(nameof(Diagnostics), "Memory free: IRAM: %u, DRAM: %u", iram, dram);

                // std::map<const char*, size_t> taskRecordings;
                // if (!GetTasksFreeStack(taskRecordings))
                // {
                //     String tasksLogString;
                //     for (auto &&recording : taskRecordings)
                //     {
                //     }
                // }

                vTaskDelay(pdMS_TO_TICKS(5 * 1000));
            }
        }
    
    public:
        Diagnostics()
        {
            ServiceEntrypointStackDepth += 512;
        }
    };
};
