#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <functional>

namespace ReadieFur::EspGps
{
    class GSMAction
    {
    public:
        enum EState
        {
            Waiting = 1 << 0,
            Processing = 1 << 1,
            Processed = 1 << 2,
            Failed = 1 << 3,
            Timeout = 1 << 4
        };

    private:
        std::function<void()>& _action;
        uint32_t _stackSize;
        TickType_t _timeout;
        EventGroupHandle_t _eventGroup = xEventGroupCreate();
        TaskHandle_t _timeoutHandle;
        TaskHandle_t _taskHandle;

        static void ProcessTaskCallback(void* param)
        {
            auto reference = reinterpret_cast<GSMAction*>(param);
            if (ulTaskNotifyTake(pdFALSE, 0) == 0) reference->_action();
            if (ulTaskNotifyTake(pdFALSE, 0) == 0) reference->UpdateState(EState::Waiting | EState::Processing, EState::Processed);
            vTaskDelete(NULL);
        }

        static void TimeoutCallback(void* param)
        {
            auto reference = reinterpret_cast<GSMAction*>(param);
            if (ulTaskNotifyTake(pdFALSE, 0) == 0) vTaskDelay(reference->_timeout);
            if (ulTaskNotifyTake(pdFALSE, 0) == 0) reference->UpdateState(EState::Waiting, EState::Timeout);
            vTaskDelete(NULL);
        }

        void UpdateState(int remove, int add)
        {
            if (remove != 0) xEventGroupClearBits(_eventGroup, remove);
            if (add != 0) xEventGroupSetBits(_eventGroup, add);
        }

    public:
        GSMAction(std::function<void()>& action, uint32_t stackSize, TickType_t timeout)
        : _action(action), _stackSize(stackSize), _timeout(timeout)
        {
            UpdateState(0, EState::Waiting);

            static char subtaskNameBuf[configMAX_TASK_NAME_LEN];
            sprintf(subtaskNameBuf, "gsmt%09d", xTaskGetTickCount());
            if (xTaskCreate(TimeoutCallback, subtaskNameBuf, configIDLE_TASK_STACK_SIZE + 128, this, configMAX_PRIORITIES * 0.4, &_timeoutHandle) != pdPASS)
                abort();
        }

        ~GSMAction()
        {
            xTaskNotifyGive(_timeoutHandle);
            xTaskNotifyGive(_taskHandle);
            vEventGroupDelete(_eventGroup);
        }

        EState GetState()
        {
            return (EState)xEventGroupGetBits(_eventGroup);
        }

        void ProcessTask()
        {
            UpdateState(EState::Waiting, EState::Processing);
            static char subtaskNameBuf[configMAX_TASK_NAME_LEN];
            sprintf(subtaskNameBuf, "gsmact%09d", xTaskGetTickCount());
            if (xTaskCreate(ProcessTaskCallback, subtaskNameBuf, _stackSize, this, configMAX_PRIORITIES * 0.4, &_taskHandle) != pdPASS)
                UpdateState(EState::Processing, EState::Failed);
        }

        bool WaitForState(EState state, TickType_t timeout)
        {
            int bits = xEventGroupWaitBits(_eventGroup, state, pdFALSE, pdFALSE, timeout);
            return bits & state;
        }
    };
};
