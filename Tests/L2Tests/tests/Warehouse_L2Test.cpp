/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include "L2Tests.h"
#include "L2TestsMock.h"
#include <condition_variable>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <interfaces/IWarehouse.h>
#include <mutex>

#define JSON_TIMEOUT (1000)
#define COM_TIMEOUT (100)
#define TEST_LOG(x, ...)                                                                                                                                        \
    fprintf(stderr, "\033[v_secure_system1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);
#define WAREHOUSE_CALLSIGN _T("org.rdk.Warehouse.1")
#define WAREHOUSEL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IWarehouse;

typedef enum : uint32_t {
    WAREHOUSEL2TEST_RESETDONE = 0x00000001,
    WAREHOUSEL2TEST_STATE_INVALID = 0x00000000
} WarehouseL2test_async_events_t;

class WarehouseNotificationHandler : public Exchange::IWarehouse::INotification {
private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

    BEGIN_INTERFACE_MAP(Notification)
    INTERFACE_ENTRY(Exchange::IWarehouse::INotification)
    END_INTERFACE_MAP

public:
    WarehouseNotificationHandler() {}
    ~WarehouseNotificationHandler() {}

    void ResetDone(const bool success, const string& error) override
    {
        TEST_LOG("ResetDone event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("ResetDone received: %s", error.c_str());
        /* Notify the requester thread. */
        m_event_signalled |= WAREHOUSEL2TEST_RESETDONE;
        m_condition_variable.notify_one();
    }

    uint32_t WaitForRequestStatus(uint32_t timeout_ms, WarehouseL2test_async_events_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        uint32_t signalled = WAREHOUSEL2TEST_STATE_INVALID;

        while (!(expected_status & m_event_signalled)) {
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                TEST_LOG("Timeout waiting for request status event");
                break;
            }
        }
        signalled = m_event_signalled;
        return signalled;
    }
};

/**
 * @brief Internal test mock class
 *
 * Note that this is for internal test use only and doesn't mock any actual
 * concrete interface.
 */

class AsyncHandlerMock_Warehouse {
public:
    AsyncHandlerMock_Warehouse()
    {
    }

    MOCK_METHOD(void, resetDone, (const JsonObject& message));
};

/* WareHouse L2 test class declaration */
class Warehouse_L2Test : public L2TestMocks {
protected:
    Core::JSONRPC::Message message;
    string response;
    IARM_EventHandler_t whMgrStatusChangeEventsHandler = nullptr;

    virtual ~Warehouse_L2Test() override;

public:
    Warehouse_L2Test();
    uint32_t CreateWarehouseInterfaceObject();
    void resetDone(const JsonObject& message);
    /**
     * @brief waits for various status change on asynchronous calls
     */
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, WarehouseL2test_async_events_t expected_status);

private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell* m_controller_warehouse;

    /** @brief Pointer to the IWarehouse interface */
    Exchange::IWarehouse* m_warehouseplugin;

    Core::Sink<WarehouseNotificationHandler> notify;
};

/**
 * @brief Constructor for WareHouse L2 test class
 */
Warehouse_L2Test::Warehouse_L2Test()
    : L2TestMocks()
    , m_controller_warehouse(nullptr)
    , m_warehouseplugin(nullptr)
{
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = WAREHOUSEL2TEST_STATE_INVALID;

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.Warehouse");
    EXPECT_EQ(Core::ERROR_NONE, status);

    if (CreateWarehouseInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid DeviceDiagnostics_Client");
    } else {
        EXPECT_TRUE(m_controller_warehouse != nullptr);
        if (m_controller_warehouse) {
            EXPECT_TRUE(m_warehouseplugin != nullptr);
            if (m_warehouseplugin) {
                m_warehouseplugin->Register(&notify);
            } else {
                TEST_LOG("m_warehouseplugin is NULL");
            }
        } else {
            TEST_LOG("m_controller_warehouse is NULL");
        }
    }
}

/**
 * @brief Destructor for WareHouse L2 test class
 */
Warehouse_L2Test::~Warehouse_L2Test()
{
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = WAREHOUSEL2TEST_STATE_INVALID;

    if (m_warehouseplugin) {
        m_warehouseplugin->Unregister(&notify);
        m_warehouseplugin->Release();
    }

    /* Deactivate plugin in destructor */
    status = DeactivateService("org.rdk.Warehouse");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 * @brief called when resetDone
 *  notification received from IARM
 *
 * @param[in] message from WareHouse on the change
 */
void Warehouse_L2Test::resetDone(const JsonObject& message)
{
    TEST_LOG("resetDone event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    message.ToString(str);

    TEST_LOG("resetDone received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= WAREHOUSEL2TEST_RESETDONE;
    m_condition_variable.notify_one();
}

/**
 * @brief waits for various status change on asynchronous calls
 *
 * @param[in] timeout_ms timeout for waiting
 */
uint32_t Warehouse_L2Test::WaitForRequestStatus(uint32_t timeout_ms, WarehouseL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::seconds timeout(timeout_ms);
    uint32_t signalled = WAREHOUSEL2TEST_STATE_INVALID;

    while (!(expected_status & m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }

    signalled = m_event_signalled;

    return signalled;
}

MATCHER_P(MatchRequest, data, "")
{
    bool match = true;
    std::string expected;
    std::string actual;

    data.ToString(expected);
    arg.ToString(actual);
    TEST_LOG(" rec = %s, arg = %s", expected.c_str(), actual.c_str());
    EXPECT_STREQ(expected.c_str(), actual.c_str());

    return match;
}

uint32_t Warehouse_L2Test::CreateWarehouseInterfaceObject()
{
    uint32_t return_value = Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> Warehouse_Engine;
    Core::ProxyType<RPC::CommunicatorClient> Warehouse_Client;

    TEST_LOG("Creating Warehouse_Engine");
    Warehouse_Engine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    Warehouse_Client = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(Warehouse_Engine));

    TEST_LOG("Creating Warehouse_Engine Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    Warehouse_Engine->Announcements(mWarehouse_Client->Announcement());
#endif
    if (!Warehouse_Client.IsValid()) {
        TEST_LOG("Invalid Warehouse_Client");
    } else {
        m_controller_warehouse = Warehouse_Client->Open<PluginHost::IShell>(_T("org.rdk.Warehouse"), ~0, 3000);
        if (m_controller_warehouse) {
            m_warehouseplugin = m_controller_warehouse->QueryInterface<Exchange::IWarehouse>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

/********************************************************
************Test case Details **************************
** 1. Triggered resetDevice Method for
** 2. Verify the response of resetDevice Method using Comrpc
** 3. Verify the event Factory resetDone getting triggered using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_Clear_True_ResetDone)
{
    uint32_t status = Core::ERROR_NONE;
    uint32_t signalled = WAREHOUSEL2TEST_STATE_INVALID;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh WAREHOUSE_CLEAR --suppressReboot"));
                return Core::ERROR_NONE;
            }));

    bool supress = true;
    string resetType = "WAREHOUSE_CLEAR";
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
    signalled = notify.WaitForRequestStatus(COM_TIMEOUT, WAREHOUSEL2TEST_RESETDONE);
    EXPECT_TRUE(signalled & WAREHOUSEL2TEST_RESETDONE);
}

/********************************************************
************Test case Details **************************
** 1. Triggered resetDevice Method for
** 2. Verify the response of resetDevice Method using jsonrpc
** 3. Verify the event Factory resetDone getting triggered using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, Warehouse_Clear_True_ResetDone)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Warehouse> async_handler;
    uint32_t signalled = WAREHOUSEL2TEST_STATE_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    std::string message;
    JsonObject expected_status;

    message = "{\"success\":true,\"error\":\"\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, resetDone(MatchRequest(expected_status)))
        .WillOnce(Invoke(this, &Warehouse_L2Test::resetDone));

    /* errorCode and errorDescription should not be set */
    EXPECT_FALSE(result.HasLabel("errorCode"));
    EXPECT_FALSE(result.HasLabel("errorDescription"));

    /* Register for resetDone event. */
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("resetDone"),
        &AsyncHandlerMock_Warehouse::resetDone,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh WAREHOUSE_CLEAR --suppressReboot"));
                return Core::ERROR_NONE;
            }));

    /* resetDevice method takes 2 parameters with different Inputs as
     *   "suppressReboot = true" & "resetType = WAREHOUSE_CLEAR"
     *   WareHouseResetIARM: WAREHOUSE_CLEAR reset...
     *   resetDeviceWrapper: response={"success":true}
     */
    params["suppressReboot"] = true;
    params["resetType"] = "WAREHOUSE_CLEAR";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    signalled = WaitForRequestStatus(JSON_TIMEOUT, WAREHOUSEL2TEST_RESETDONE);
    EXPECT_TRUE(signalled & WAREHOUSEL2TEST_RESETDONE);

    /* Unregister for events. */
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("resetDone"));
}

/********************************************************
************Test case Details **************************
** 1. Triggered lightReset Method
** 2. Verify the response of lightReset Method using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_LightReset)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh -c 'rm -rf /opt/netflix/* SD_CARD_MOUNT_PATH/netflix/* XDG_DATA_HOME/* XDG_CACHE_HOME/* XDG_CACHE_HOME/../.sparkStorage/ /opt/QT/home/data/* /opt/hn_service_settings.conf /opt/apps/common/proxies.conf /opt/lib/bluetooth /opt/persistent/rdkservicestore'"));
                return Core::ERROR_NONE;
            }));

    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->LightReset(response);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
}
/********************************************************
************Test case Details **************************
** 1. Set environment variables
** 1. Triggered lightReset Method
** 2. Verify the response of lightReset Method using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_LightReset_SetENV)
{
    uint32_t status = Core::ERROR_NONE;

    // Set environment variables before invoking the function
    setenv("SD_CARD_MOUNT_PATH", "/mnt/sdcard", 1);
    setenv("XDG_DATA_HOME", "/home/user/.local/share", 1);
    setenv("XDG_CACHE_HOME", "/home/user/.cache", 1);

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh -c 'rm -rf /opt/netflix/* SD_CARD_MOUNT_PATH/netflix/* XDG_DATA_HOME/* XDG_CACHE_HOME/* XDG_CACHE_HOME/../.sparkStorage/ /opt/QT/home/data/* /opt/hn_service_settings.conf /opt/apps/common/proxies.conf /opt/lib/bluetooth /opt/persistent/rdkservicestore'"));
                return Core::ERROR_NONE;
            }));

    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->LightReset(response);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);

    // Clean up environment variables after the test
    unsetenv("SD_CARD_MOUNT_PATH");
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CACHE_HOME");
}

/********************************************************
************Test case Details **************************
** 1. Triggered lightReset Method
** 2. Verify the response of lightReset Method using jsonrpc
*******************************************************/
TEST_F(Warehouse_L2Test, Warehouse_LightReset)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    status = InvokeServiceMethod("org.rdk.Warehouse.1", "lightReset", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                return Core::ERROR_NONE;
            }))
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh -c 'rm -rf /opt/netflix/* SD_CARD_MOUNT_PATH/netflix/* XDG_DATA_HOME/* XDG_CACHE_HOME/* XDG_CACHE_HOME/../.sparkStorage/ /opt/QT/home/data/* /opt/hn_service_settings.conf /opt/apps/common/proxies.conf /opt/lib/bluetooth /opt/persistent/rdkservicestore'"));
                return Core::ERROR_NONE;
            }));

    /* errorCode and errorDescription should not be set */
    EXPECT_FALSE(result.HasLabel("errorCode"));
    EXPECT_FALSE(result.HasLabel("errorDescription"));

    /* lightReset method takes no parameters */
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "lightReset", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    // lightReset: lightReset succeeded
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "lightReset", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/********************************************************
************Test case Details **************************
** 1. Triggered getHardwareTestResults Method
** 2. Verify the response of getHardwareTestResults Method using comrpc
** 3. Triggered executeHardwareTest Method
** 4. Verify the response of executeHardwareTest Method using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_getHardwareTestResults_executeHardwareTest)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                EXPECT_EQ(string(pcCallerID), string("Warehouse"));
                EXPECT_EQ(string(pcParameterName), string("Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.Results"));
                strncpy(pstParamData->value, "test", sizeof(pstParamData->value));
                return WDMP_SUCCESS;
            }));

    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("false"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);
                return WDMP_SUCCESS;
            }));

    bool success;
    string response;
    status = m_warehouseplugin->GetHardwareTestResults(success, response);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_STREQ("test", response.c_str());

    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("true"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);
                return WDMP_SUCCESS;
            }))
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.ExecuteTest"));
                EXPECT_EQ(string(pcParameterValue), _T("1"));
                EXPECT_EQ(eDataType, WDMP_INT);
                return WDMP_SUCCESS;
            }));

    Exchange::IWarehouse::WarehouseSuccess waresuccess;
    status = m_warehouseplugin->ExecuteHardwareTest(waresuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(waresuccess.success);

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                EXPECT_EQ(string(pcCallerID), string("Warehouse"));
                EXPECT_EQ(string(pcParameterName), string("Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.Results"));
                strncpy(pstParamData->value, "test", sizeof(pstParamData->value));
                return WDMP_SUCCESS;
            }));
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("false"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);
                return WDMP_SUCCESS;
            }));

    string param = "Timezone: NA 2021-04-15 10:35:06 Test execution start, remote trigger ver. 0011 2021-04-15 10:35:10 Test result: Audio/Video Decoder:PASSED 2021-04-15 10:35:06 Test result: Dynamic RAM:PASSED 2021-04-15 10:35:06 Test result: Flash Memory:PASSED 2021-04-15 10:35:06 Test result: HDMI Output:PASSED 2021-04-15 10:35:38 Test result: IR Remote Interface:WARNING_IR_Not_Detected 2021-04-15 10:35:06 Test result: Bluetooth:PASSED 2021-04-15 10:35:06 Test result: SD Card:PASSED 2021-04-15 10:35:06 Test result: WAN:PASSED 2021-04-15 10:35:38 Test execution completed:PASSED";
    status = m_warehouseplugin->GetHardwareTestResults(success, response);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_STREQ("test", response.c_str());
}

/********************************************************
************Test case Details **************************
** 1. Triggered getHardwareTestResults Method
** 2. Verify the response of getHardwareTestResults Method using jsonrpc
** 3. Triggered executeHardwareTest Method
** 4. Verify the response of executeHardwareTest Method using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_getHardwareTestResults_executeHardwareTest)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                EXPECT_EQ(string(pcCallerID), string("Warehouse"));
                EXPECT_EQ(string(pcParameterName), string("Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.Results"));
                strncpy(pstParamData->value, "test", sizeof(pstParamData->value));
                return WDMP_SUCCESS;
            }));

    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("false"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);
                return WDMP_SUCCESS;
            }));

    // getHardwareTestResultsWrapper: response={"testResults":"test","success":true}
    params["testResults"] = "";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "getHardwareTestResults", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_STREQ("test", result["testResults"].String().c_str());

    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("true"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);
                return WDMP_SUCCESS;
            }))
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.ExecuteTest"));
                EXPECT_EQ(string(pcParameterValue), _T("1"));
                EXPECT_EQ(eDataType, WDMP_INT);
                return WDMP_SUCCESS;
            }));

    // executeHardwareTestWrapper: response={"success":true}
    params["testResults"] = "";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "executeHardwareTest", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_STREQ("true", result["success"].String().c_str());

    EXPECT_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
                EXPECT_EQ(string(pcCallerID), string("Warehouse"));
                EXPECT_EQ(string(pcParameterName), string("Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.hwHealthTest.Results"));
                strncpy(pstParamData->value, "test", sizeof(pstParamData->value));
                return WDMP_SUCCESS;
            }));
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Warehouse"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.hwHealthTest.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("false"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);
                return WDMP_SUCCESS;
            }));

    // getHardwareTestResultsWrapper: response={"testResults":"test","success":true}
    params["testResults"] = "Timezone: NA 2021-04-15 10:35:06 Test execution start, remote trigger ver. 0011 2021-04-15 10:35:10 Test result: Audio/Video Decoder:PASSED 2021-04-15 10:35:06 Test result: Dynamic RAM:PASSED 2021-04-15 10:35:06 Test result: Flash Memory:PASSED 2021-04-15 10:35:06 Test result: HDMI Output:PASSED 2021-04-15 10:35:38 Test result: IR Remote Interface:WARNING_IR_Not_Detected 2021-04-15 10:35:06 Test result: Bluetooth:PASSED 2021-04-15 10:35:06 Test result: SD Card:PASSED 2021-04-15 10:35:06 Test result: WAN:PASSED 2021-04-15 10:35:38 Test execution completed:PASSED";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "getHardwareTestResults", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_STREQ("test", result["testResults"].String().c_str());
}

/********************************************************
************Test case Details **************************
** 1. Triggered internalReset Method
** 2. Verify the response of internalReset Method using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_internalReset)
{
    uint32_t status = Core::ERROR_NONE;
    string passphrase;
    Exchange::IWarehouse::WarehouseSuccessErr response;

    // Invoke internalReset - No pass phrase
    status = m_warehouseplugin->InternalReset(passphrase, response);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(response.success);

    // Invoke internalReset - Incorrect pass phrase
    passphrase = "Incorrect pass phrase";
    status = m_warehouseplugin->InternalReset(passphrase, response);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(response.success);

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                return Core::ERROR_NONE;
            }))
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("rm -rf /opt/drm /opt/www/whitebox /opt/www/authService && /rebootNow.sh -s WarehouseService &"));
                return Core::ERROR_NONE;
            }));

    // Invoke internalReset - correct pass phrase - script Failure
    passphrase = "FOR TEST PURPOSES ONLY";
    status = m_warehouseplugin->InternalReset(passphrase, response);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(response.success);

    // Invoke internalReset - Correct pass phrase - Return success
    passphrase = "FOR TEST PURPOSES ONLY";
    status = m_warehouseplugin->InternalReset(passphrase, response);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(response.success);
}

/********************************************************
************Test case Details **************************
** 1. Triggered internalReset Method
** 2. Verify the response of internalReset Method using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_internalReset)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    // Invoke internalReset - No pass phrase
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "internalReset", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(result["success"].Boolean());

    // Invoke internalReset - Incorrect pass phrase
    params["passPhrase"] = "Incorrect pass phrase";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "internalReset", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(result["success"].Boolean());

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                return Core::ERROR_NONE;
            }))
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("rm -rf /opt/drm /opt/www/whitebox /opt/www/authService && /rebootNow.sh -s WarehouseService &"));
                return Core::ERROR_NONE;
            }));

    // Invoke internalReset - correct pass phrase - script Failure
    params["passPhrase"] = "FOR TEST PURPOSES ONLY";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "internalReset", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());

    // Invoke internalReset - Correct pass phrase - Return success
    params["passPhrase"] = "FOR TEST PURPOSES ONLY";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "internalReset", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/********************************************************
************Test case Details **************************
** 1. Triggered isClean Method
** 2. Verify the response of isClean Method using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_iscleanTest)
{
    uint32_t status = Core::ERROR_NONE;
    WPEFramework::RPC::IStringIterator* files;
    bool clean, success;
    int age;
    string error;
    const string userPrefFile = _T("/opt/user_preferences.conf");
    const uint8_t userPrefLang[] = "[General]\nui_language=US_en\n";
    const string customDataFile = _T("/lib/rdk/wh_api_5.conf");
    const uint8_t customDataFileContent[] = "[files]\n/opt/user_preferences.conf\n";

    EXPECT_TRUE(std::ifstream("/opt/user_preferences.conf").good() ? std::remove("/opt/user_preferences.conf") == 0 : true);

    // No conf file
    status = m_warehouseplugin->IsClean(age, clean, files, success, error);
    TEST_LOG("error: %s",error.c_str());
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(clean);
    EXPECT_FALSE(success);

    // Empty conf file
    Core::File fileConf(customDataFile);
    Core::Directory(fileConf.PathName().c_str()).CreatePath();
    fileConf.Create();

    age = 300;
    status = m_warehouseplugin->IsClean(age, clean, files, success, error);
    TEST_LOG("error: %s",error.c_str());
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(clean);
    EXPECT_FALSE(success);

    // Create empty conf file
    fileConf.Write(customDataFileContent, strlen(reinterpret_cast<const char*>(customDataFileContent)));

    age = 300;
    status = m_warehouseplugin->IsClean(age, clean, files, success, error);
    TEST_LOG("error: %s",error.c_str());
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(clean);
    EXPECT_TRUE(success);

    // Add test data to conf file
    Core::File filePref(userPrefFile);
    Core::Directory(filePref.PathName().c_str()).CreatePath();
    filePref.Create();
    filePref.Open();
    filePref.Write(userPrefLang, sizeof(userPrefLang));

    age = -4;
    status = m_warehouseplugin->IsClean(age, clean, files, success, error);
    TEST_LOG("error: %s",error.c_str());
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(clean);
    EXPECT_TRUE(success);

    fileConf.Destroy();
    filePref.Destroy();

    //Clean the /lib/rdk/wh_api_5.conf file after the API call
    std::ofstream clear("/lib/rdk/wh_api_5.conf", std::ios::trunc);
    clear.close();
    std::remove("/opt/user_preferences.conf");
}

/********************************************************
************Test case Details **************************
** 1. Triggered isClean Method
** 2. Verify the response of isClean Method using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_iscleanTest)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    const string userPrefFile = _T("/opt/user_preferences.conf");
    const uint8_t userPrefLang[] = "[General]\nui_language=US_en\n";
    const string customDataFile = _T("/lib/rdk/wh_api_5.conf");
    const uint8_t customDataFileContent[] = "[files]\n/opt/user_preferences.conf\n";

    // No conf file
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "isClean", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(result["success"].Boolean());
    EXPECT_FALSE(result["clean"].Boolean());
    EXPECT_STREQ("[\"\"]", result["files"].String().c_str());

    // Empty conf file
    Core::File fileConf(customDataFile);
    Core::Directory(fileConf.PathName().c_str()).CreatePath();
    fileConf.Create();

    params["age"] = 300;
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "isClean", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_FALSE(result["success"].Boolean());
    EXPECT_FALSE(result["clean"].Boolean());
    EXPECT_STREQ("[\"\"]", result["files"].String().c_str());

    // Create empty conf file
    fileConf.Write(customDataFileContent, sizeof(customDataFileContent));

    params["age"] = 300;
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "isClean", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_TRUE(result["clean"].Boolean());
    EXPECT_STREQ("[\"\"]", result["files"].String().c_str());

    // Add test data to conf file
    Core::File filePref(userPrefFile);
    Core::Directory(filePref.PathName().c_str()).CreatePath();
    filePref.Create();
    filePref.Open();
    filePref.Write(userPrefLang, sizeof(userPrefLang));

    params["age"] = -4;
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "isClean", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_FALSE(result["clean"].Boolean());
    EXPECT_STREQ("[\"\\/opt\\/user_preferences.conf\"]", result["files"].String().c_str());

    fileConf.Destroy();
    filePref.Destroy();
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Update device properties to validate IsClean API.
*******************************************************/
TEST_F(Warehouse_L2Test, Write_To_DeviceProperties)
{
    // Create file so that it exists in the system before calling isClean
    Core::Directory("/opt/test_dir").CreatePath();
    Core::File("/opt/test_dir/file1.txt").Create();

    // Write ENV Variable to device.properties
    Core::File props("/etc/device.properties");
    props.Create();
    props.Write(reinterpret_cast<const uint8_t*>("TEST_PATH=/opt/test_dir\n"), strlen("TEST_PATH=/opt/test_dir\n"));
    props.Close();

    // Write conf with multiple files
    std::ofstream config("/lib/rdk/wh_api_5.conf");
    config << "[files]\n";
    config << "$TEST_PATH/*";
    config.close();

    int age = 100;
    bool clean = false, success = false;
    string error;
    WPEFramework::RPC::IStringIterator* files = nullptr;

    uint32_t status = m_warehouseplugin->IsClean(age, clean, files, success, error);
    TEST_LOG("error: %s",error.c_str());
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(success);

    //Clean the file after the API call
    std::ofstream clear("/lib/rdk/wh_api_5.conf", std::ios::trunc);
    clear.close();
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=COLD) using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_Cold_Factory)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh coldfactory"));
                return Core::ERROR_NONE;
            }))
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string(" /rebootNow.sh -s PowerMgr_coldFactoryReset -o 'Rebooting the box due to Cold Factory Reset process ...'"));
                return Core::ERROR_NONE;
            }));

    bool supress = true;
    string resetType = "COLD";
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    sleep(6);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=COLD) using jsonrpc
*******************************************************/
TEST_F(Warehouse_L2Test, Warehouse_Cold_Factory)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh coldfactory"));
                return Core::ERROR_NONE;
            }))
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string(" /rebootNow.sh -s PowerMgr_coldFactoryReset -o 'Rebooting the box due to Cold Factory Reset process ...'"));
                return Core::ERROR_NONE;
            }));

    params["suppressReboot"] = true;
    params["resetType"] = "COLD";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    sleep(6); // This sleep is needed since the plugin code has a sleep of 5 seconds
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=FACTORY) using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_Factory_ResetDevice)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh factory"));
                return Core::ERROR_NONE;
            }));

    bool supress = true;
    string resetType = "FACTORY";
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    sleep(6);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=FACTORY) using jsonrpc
*******************************************************/
TEST_F(Warehouse_L2Test, Warehouse_Factory_ResetDevice)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh factory"));
                return Core::ERROR_NONE;
            }));
    //.WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    params["suppressReboot"] = true;
    params["resetType"] = "FACTORY";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=USERFACTORY) using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_UserFactory_ResetDevice)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh userfactory"));
                return Core::ERROR_NONE;
            }));

    bool supress = true;
    string resetType = "USERFACTORY";
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    sleep(6);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
}
/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=USERFACTORY) using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_UserFactory_ResetDevice)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh userfactory"));
                return Core::ERROR_NONE;
            }));

    params["suppressReboot"] = true;
    params["resetType"] = "USERFACTORY";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=FALSE, resetType=WAREHOUSE_CLEAR) using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_False_Clear_ResetDevice)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh WAREHOUSE_CLEAR"));
                return Core::ERROR_NONE;
            }));

    bool supress = false;
    string resetType = "WAREHOUSE_CLEAR";
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    sleep(6);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=FALSE, resetType=WAREHOUSE_CLEAR) using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_False_Clear_ResetDevice)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh WAREHOUSE_CLEAR"));
                return Core::ERROR_NONE;
            }));

    params["suppressReboot"] = false;
    params["resetType"] = "WAREHOUSE_CLEAR";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=WAREHOUSE_CLEAR) using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_Clear_ResetDevice)
{
    uint32_t status = Core::ERROR_NONE;
    uint32_t signalled = WAREHOUSEL2TEST_STATE_INVALID;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh WAREHOUSE_CLEAR --suppressReboot"));
                return Core::ERROR_NONE;
            }));

    bool supress = true;
    string resetType = "WAREHOUSE_CLEAR";
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    sleep(6);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);

    signalled = notify.WaitForRequestStatus(COM_TIMEOUT, WAREHOUSEL2TEST_RESETDONE);
    EXPECT_TRUE(signalled & WAREHOUSEL2TEST_RESETDONE);
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=TRUE, resetType=WAREHOUSE_CLEAR) using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_Clear_ResetDevice)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_Warehouse> async_handler;
    uint32_t signalled = WAREHOUSEL2TEST_STATE_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    std::string message;
    JsonObject expected_status;

    message = "{\"success\":true,\"error\":\"\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, resetDone(MatchRequest(expected_status)))
        .WillOnce(Invoke(this, &Warehouse_L2Test::resetDone));

    /* errorCode and errorDescription should not be set */
    EXPECT_FALSE(result.HasLabel("errorCode"));
    EXPECT_FALSE(result.HasLabel("errorDescription"));

    /* Register for resetDone event. */
    status = jsonrpc.Subscribe<JsonObject>(JSON_TIMEOUT,
        _T("resetDone"),
        &AsyncHandlerMock_Warehouse::resetDone,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh WAREHOUSE_CLEAR --suppressReboot"));
                return Core::ERROR_NONE;
            }));

    params["suppressReboot"] = true;
    params["resetType"] = "WAREHOUSE_CLEAR";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    EXPECT_TRUE(result["success"].Boolean());

    signalled = WaitForRequestStatus(JSON_TIMEOUT, WAREHOUSEL2TEST_RESETDONE);
    EXPECT_TRUE(signalled & WAREHOUSEL2TEST_RESETDONE);

    /* Unregister for events. */
    jsonrpc.Unsubscribe(JSON_TIMEOUT, _T("resetDone"));
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=false, resetType="") using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_Generic_ResetDevice)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh warehouse"));
                return Core::ERROR_NONE;
            }));

    bool supress = false;
    string resetType;
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    sleep(6);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=false, resetType="") using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_Generic_ResetDevice)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh warehouse"));
                return Core::ERROR_NONE;
            }));
    params["suppressReboot"] = "false";
    params["resetType"] = "";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    EXPECT_TRUE(result["success"].Boolean());
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=true, resetType="") using comrpc
*******************************************************/
TEST_F(Warehouse_L2Test, COMRPC_Warehouse_UserFactory_ResetDevice_Failure)
{
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh warehouse --suppressReboot &"));
                return Core::ERROR_GENERAL;
            }));

    bool supress = true;
    string resetType;
    Exchange::IWarehouse::WarehouseSuccessErr response;
    status = m_warehouseplugin->ResetDevice(supress, resetType, response);
    sleep(6);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(response.success);
}

/********************************************************
************Test case Details **************************
** 1. TEST_F to achieve max. Lcov
** 2. Triggered & Verify the resetDevice Method with params (suppressReboot=true, resetType="") using jsonrpc
*******************************************************/

TEST_F(Warehouse_L2Test, Warehouse_UserFactory_ResetDevice_Failure)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(WAREHOUSE_CALLSIGN, WAREHOUSEL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    EXPECT_CALL(*p_wrapsImplMock, v_secure_system(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const char* command, va_list args) {
                EXPECT_EQ(string(command), string("sh /lib/rdk/deviceReset.sh warehouse --suppressReboot &"));
                return Core::ERROR_GENERAL;
            }));

    params["suppressReboot"] = true;
    params["resetType"] = "";
    status = InvokeServiceMethod("org.rdk.Warehouse.1", "resetDevice", params, result);
    EXPECT_TRUE(result["success"].Boolean());
}