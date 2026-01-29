/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/IWarehouse.h>

#include <com/com.h>
#include <core/core.h>

#include <thread>

#include "UtilsThreadRAII.h"
#include "libIARM.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class WarehouseImplementation : public Exchange::IWarehouse
        {
            public:
                // We do not allow this plugin to be copied !!
                WarehouseImplementation();
                ~WarehouseImplementation() override;

                // We do not allow this plugin to be copied !!
                WarehouseImplementation(const WarehouseImplementation&) = delete;
                WarehouseImplementation& operator=(const WarehouseImplementation&) = delete;

                BEGIN_INTERFACE_MAP(WarehouseImplementation)
                INTERFACE_ENTRY(Exchange::IWarehouse)
                END_INTERFACE_MAP

            public:
                enum Event
                {
                    WAREHOUSE_EVT_RESET_DONE
                };
 
            class EXTERNAL Job : public Core::IDispatch {
            protected:
                Job(WarehouseImplementation* warehouseImplementation, Event event, JsonObject& params)
                    : _warehouseImplementation(warehouseImplementation)
                    , _event(event)
                    , _params(params) {
                    if (_warehouseImplementation != nullptr) {
                        _warehouseImplementation->AddRef();
                    }
                }

            public:
                Job() = delete;
                Job(const Job&) = delete;
                Job& operator=(const Job&) = delete;
                ~Job() {
                    if (_warehouseImplementation != nullptr) {
                        _warehouseImplementation->Release();
                    }
                }

            public:
                static Core::ProxyType<Core::IDispatch> Create(WarehouseImplementation* warehouseImplementation, Event event, JsonObject params) {
#ifndef USE_THUNDER_R4
                    return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(warehouseImplementation, event, params)));
#else
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(warehouseImplementation, event, params)));
#endif
                }

                virtual void Dispatch() {
                    _warehouseImplementation->Dispatch(_event, _params);
                }

            private:
                WarehouseImplementation *_warehouseImplementation;
                const Event _event;
                const JsonObject _params;
        };
        public:
            virtual Core::hresult Register(Exchange::IWarehouse::INotification *notification ) override ;
            virtual Core::hresult Unregister(Exchange::IWarehouse::INotification *notification ) override;

            Core::hresult ExecuteHardwareTest(WarehouseSuccess& success) override;
            Core::hresult GetHardwareTestResults(bool& success, string& testResults) override;
            Core::hresult InternalReset(const string& passPhrase, WarehouseSuccessErr& successErr) override;
            Core::hresult IsClean(const int age, bool &clean, IStringIterator*& files, bool &success, string& error) override;
            Core::hresult LightReset(WarehouseSuccessErr& successErr) override;
            Core::hresult ResetDevice(const bool suppressReboot, const string& resetType, WarehouseSuccessErr& successErr) override;

        private:
            mutable Core::CriticalSection _adminLock;
            std::list<Exchange::IWarehouse::INotification*> _warehouseNotification;
            Utils::ThreadRAII m_resetThread;
            
            void dispatchEvent(Event, const JsonObject &params);
            void Dispatch(Event event, const JsonObject &params);
            static void dsWareHouseOpnStatusChanged(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
            /*gets the SD card mount path by reading /proc/mounts, returns true on success, false otherwise*/
            bool getSDCardMountPath(string&);
            /*Adds 0 to file /opt/.rebootFlag*/
            void resetWarehouseRebootFlag();
            void InitializeIARM();
            void DeinitializeIARM();
            
        public:
            static WarehouseImplementation* _instance;
            
            void ResetDone(bool success, string error);
            uint32_t processColdFactoryReset();
            uint32_t processFactoryReset();
            uint32_t processWareHouseReset();
            uint32_t processWHReset();
            uint32_t processWHResetNoReboot();
            uint32_t processWHClear();
            uint32_t processWHClearNoReboot();
            uint32_t processUserFactoryReset();

            friend class Job;
        };
    } // namespace Plugin
} // namespace WPEFramework
