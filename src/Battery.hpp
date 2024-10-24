#pragma once

#include <Arduino.h>
#include "Board.h"
#include <esp_sleep.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>
#include "Service/AService.hpp"
#include "Storage.hpp"
#include "Helpers.h"
#include "Logging.hpp"
#include <Event/Event.hpp>

namespace ReadieFur::EspGps
{
    class Battery : public Service::AService
    {
    public:
        enum EState
        {
            Charging = 1,
            Discharging = 2,
            // Full = 4,
            // Ok = 8,
            Low = 16,
            Critical = 32
        };

    private:
        std::mutex _mutex;
        uint32_t _voltage = 0, _chargeVoltage = 0;
        EState _state = EState::Charging; //Assume we are plugged in by default.

        static uint SampleADC(int adcPin)
        {
            //Calculate the average power data.
            std::vector<uint32_t> data;
            for (int i = 0; i < 30; ++i)
            {
                //TODO: Detect that the battery is charging if the average of these samples keeps increasing steadily.
                uint32_t val = analogReadMilliVolts(adcPin);
                //SerialMon.printf("analogReadMilliVolts : %u mv \n", val * 2);
                data.push_back(val);
                delay(30);
            }
            std::sort(data.begin(), data.end());
            data.erase(data.begin());
            data.pop_back();

            int sum = std::accumulate(data.begin(), data.end(), 0);
            double average = static_cast<double>(sum) / data.size();
            average *= 2;

            return average;
        }

        void UpdateVoltage()
        {
            _mutex.lock();

            _voltage = SampleADC(BATTERY_ADC);
            #ifdef CHARGE_ADC
            _chargeVoltage = SampleADC(CHARGE_ADC);
            #endif

            EState oldState = _state;

            if (_voltage <= BATTERY_CRIT_VOLTAGE)
                _state = EState::Critical;
            else if (_voltage <= BATTERY_LOW_VOLTAGE)
                _state = EState::Low;

            _state = (EState)(_state | (_chargeVoltage >= CHG_VOLTAGE_MIN ? EState::Charging : EState::Discharging));

            if (oldState != _state)
                OnStateChanged.Dispatch(_state);

            _mutex.unlock();
        }

    protected:
        void RunServiceImpl() override
        {
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                UpdateVoltage();
                LOGV(nameof(Battery), "Voltage: Battery: %u, Charge: %u", _voltage, _chargeVoltage);
                vTaskDelay(pdMS_TO_TICKS(5 * 1000));
            }
        }

    public:
        Event::Event<EState> OnStateChanged;

        Battery()
        {
            ServiceEntrypointStackDepth += 1024;
        }

        uint GetSleepDuration()
        {
            switch (_state)
            {
            case EState::Discharging:
                return GetConfig(int, BATTERY_OK_INTERVAL);
            case EState::Discharging | EState::Low:
                return GetConfig(int, BATTERY_LOW_INTERVAL);
            case EState::Discharging | EState::Critical:
                return GetConfig(int, BATTERY_CRIT_SLEEP);
            default:
                //All charging states for now will use the single charging value.
                return GetConfig(int, BATTERY_CHRG_INTERVAL);
            }
        }

        void GetStatus(double* batteryVoltage, double* chargeVoltage, EState* state)
        {
            if (batteryVoltage != nullptr) *batteryVoltage = _voltage;
            if (chargeVoltage != nullptr) *chargeVoltage = _chargeVoltage;
            if (state != nullptr) *state = _state;
        }
    };
};
