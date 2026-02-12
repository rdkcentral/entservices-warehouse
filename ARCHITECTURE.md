# Warehouse Plugin Architecture

## Overview

The Warehouse plugin is a WPEFramework (Thunder) service plugin that provides device warehouse management capabilities for RDK-based devices. It enables factory reset operations, device state verification, hardware testing, and warehouse mode operations essential for device provisioning, maintenance, and return-to-factory scenarios.

## System Architecture

### Component Structure

The Warehouse plugin follows the WPEFramework plugin architecture with the following key components:

```
entservices-warehouse/
├── plugin/                    # Core plugin implementation
│   ├── Warehouse.cpp         # Plugin interface and JSON-RPC layer
│   ├── Warehouse.h           # Plugin class definition
│   ├── WarehouseImplementation.cpp  # Business logic implementation
│   ├── WarehouseImplementation.h    # Implementation interface
│   └── Module.cpp            # Plugin module registration
├── helpers/                   # Utility libraries
│   ├── UtilsIarm.h           # IARM bus communication utilities
│   ├── UtilsJsonRpc.h        # JSON-RPC helper functions
│   ├── UtilsString.h         # String manipulation utilities
│   ├── frontpanel.cpp/.h     # Front panel LED control
│   └── tptimer.h             # Timer utilities
├── cmake/                     # Build configuration
│   ├── FindDS.cmake          # DeviceSettings library finder
│   └── FindIARMBus.cmake     # IARM bus library finder
└── Tests/                     # Test suites
    ├── L1Tests/              # Unit tests
    └── L2Tests/              # Integration tests
```

### Architectural Layers

#### 1. Plugin Interface Layer (Warehouse.cpp/.h)

The plugin interface layer implements the WPEFramework plugin contracts:
- **IPlugin Interface**: Lifecycle management (Initialize/Deinitialize)
- **JSONRPC Interface**: Exposes REST API endpoints for external clients
- **IWarehouse Interface**: COM-RPC interface for internal service communication
- **Notification System**: Event broadcasting for asynchronous operations

The Warehouse class acts as a thin facade that delegates business logic to WarehouseImplementation while handling:
- Plugin activation/deactivation
- JSON-RPC method registration and routing
- Client connection lifecycle management
- Event notification to registered subscribers

#### 2. Implementation Layer (WarehouseImplementation.cpp/.h)

The core business logic layer provides:

**Reset Operations**:
- `ResetDevice()`: Full factory reset with optional reboot suppression
- `InternalReset()`: Secure internal reset with passphrase verification
- `LightReset()`: User data cleanup without full factory reset
- Multiple reset type support: WAREHOUSE, CUSTOMER_FACTORY, COLD, USER_FACTORY

**Device State Management**:
- `IsClean()`: Verifies device cleanliness by checking for residual files
- `ExecuteHardwareTest()`: Initiates hardware validation tests
- `GetHardwareTestResults()`: Retrieves hardware test status and results

**IARM Integration**:
- Listens to Device Settings warehouse operation status events
- Communicates with System Manager for reset operations
- Coordinates with front panel for visual feedback

#### 3. Communication Layer

**IARM Bus Communication**:
- Uses IARM (Integrated Application Runtime Manager) for inter-process communication
- Communicates with sysMgr for system-level reset operations
- Receives events from Device Settings (dsWareHouseOpnStatusChanged)
- Thread-safe event dispatching and notification

**JSON-RPC Protocol**:
- RESTful API over HTTP/WebSocket
- Asynchronous operation support with event notifications
- Standard Thunder JSON-RPC 2.0 format

#### 4. Helper Utilities Layer

Provides reusable components:
- **IARM Utilities**: Simplified IARM initialization and event handling
- **JSON-RPC Utilities**: Response formatting and error handling
- **String Utilities**: Safe string operations and conversions
- **Front Panel Control**: LED status indication for warehouse operations
- **File System Utilities**: File existence checks and content reading

## Data Flow

### Reset Operation Flow

```
Client Request → JSON-RPC → Warehouse Plugin → WarehouseImplementation
                                                       ↓
                                                  IARM Bus
                                                       ↓
                                              System Manager
                                                       ↓
                                              Reset Execution
                                                       ↓
                                              Event Callback
                                                       ↓
                                          Warehouse Plugin
                                                       ↓
                                          ResetDone Event
                                                       ↓
                                          Client Notification
```

### Hardware Test Flow

```
Client → ExecuteHardwareTest() → RFC Configuration Check
                                         ↓
                                  Initiate Test via RFC
                                         ↓
                                  Return Immediate Success
                                         
Client → GetHardwareTestResults() → Query RFC Results
                                         ↓
                                  Return Test Status
```

## Plugin Framework Integration

The Warehouse plugin integrates with WPEFramework through:

1. **Service Registration**: Registered as a COM service with ID `Exchange::IWarehouse`
2. **Plugin Lifecycle**: Managed by PluginHost controller
3. **Configuration**: Loaded via Thunder configuration JSON with plugin-specific settings
4. **Interface Exposure**: Both COM-RPC (internal) and JSON-RPC (external) interfaces

## Dependencies

### System Libraries
- **WPEFramework Core**: Plugin infrastructure and base classes
- **WPEFramework Interfaces**: IWarehouse and related exchange interfaces
- **DeviceSettings (DS)**: Hardware abstraction for device capabilities
- **IARM Bus**: Inter-process communication framework

### RDK Components
- **System Manager**: System-level operations and reboot control
- **RFC (Remote Feature Control)**: Dynamic configuration management
- **Front Panel HAL**: LED and display control

### Build Dependencies
- CMake 3.3+
- Thunder R4.4.1+
- CompileSettingsDebug
- C++11 compiler

## Thread Safety

The implementation uses:
- **Critical Sections** (`_adminLock`): Protects notification list access
- **Thread-safe IARM**: IARM bus handles internal thread synchronization
- **RAII Thread Management** (`Utils::ThreadRAII`): Safe thread lifecycle for reset operations
- **Atomic Event Dispatch**: Core::IDispatch job queue for event serialization

## Error Handling

- Uses WPEFramework Core::hresult return codes
- Comprehensive error reporting through `WarehouseSuccessErr` structures
- Validation of input parameters (passphrase, reset types)
- Graceful degradation when optional dependencies unavailable
- Detailed logging at all levels (INFO, WARN, ERROR)

## Extensibility

The plugin architecture supports extension through:
- Additional reset types via resetType parameter
- Pluggable notification handlers via INotification interface
- RFC-based feature enablement without code changes
- Custom hardware test implementations

## Performance Considerations

- Asynchronous reset operations prevent blocking
- Minimal memory footprint with singleton implementation
- Efficient IARM event filtering
- Lazy initialization of optional components
- Background thread for time-consuming operations
