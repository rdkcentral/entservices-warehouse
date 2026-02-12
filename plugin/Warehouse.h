/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
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
 **/

#pragma once

#include "Module.h"
#include <interfaces/IWarehouse.h>
#include <interfaces/json/JWarehouse.h>
#include <interfaces/json/JsonData_Warehouse.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework 
{
    namespace Plugin
    {
        class Warehouse : public PluginHost::IPlugin, public PluginHost::JSONRPC 
        {
            private:
                class Notification : public RPC::IRemoteConnection::INotification, public Exchange::IWarehouse::INotification
                {
                    private:
                        Notification() = delete;
                        Notification(const Notification&) = delete;
                        Notification& operator=(const Notification&) = delete;

                    public:
                    explicit Notification(Warehouse* parent) 
                        : _parent(*parent)
                        {
                            ASSERT(parent != nullptr);
                        }

                        virtual ~Notification()
                        {
                        }

                        BEGIN_INTERFACE_MAP(Notification)
                        INTERFACE_ENTRY(Exchange::IWarehouse::INotification)
                        INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                        END_INTERFACE_MAP

                        void Activated(RPC::IRemoteConnection*) override
                        {
                           
                        }

                        void Deactivated(RPC::IRemoteConnection *connection) override
                        {
                            _parent.Deactivated(connection);
                        }

                        void ResetDone(const bool success, const string& error) override
                        {
                            LOGINFO("ResetDone");
                            Exchange::JWarehouse::Event::ResetDone(_parent, success,error);
                        }

                    private:
                        Warehouse& _parent;
                };

                public:
                    Warehouse(const Warehouse&) = delete;
                    Warehouse& operator=(const Warehouse&) = delete;

                    Warehouse();
                    virtual ~Warehouse();

                    BEGIN_INTERFACE_MAP(Warehouse)
                    INTERFACE_ENTRY(PluginHost::IPlugin)
                    INTERFACE_ENTRY(PluginHost::IDispatcher)
                    INTERFACE_AGGREGATE(Exchange::IWarehouse, _warehouse)
                    END_INTERFACE_MAP

                    //  IPlugin methods
                    // -------------------------------------------------------------------------------------------------------
                    const string Initialize(PluginHost::IShell* service) override;
                    void Deinitialize(PluginHost::IShell* service) override;
                    string Information() const override;

                private:
                    void Deactivated(RPC::IRemoteConnection* connection);

                private:
                    PluginHost::IShell* _service{};
                    uint32_t _connectionId{};
                    Exchange::IWarehouse* _warehouse{};
                    Core::Sink<Notification> _warehouseNotification;
       };
    } // namespace Plugin
} // namespace WPEFramework
