# Warehouse Plugin - Product Overview

## Product Description

The Warehouse plugin is a critical enterprise service for RDK (Reference Design Kit) devices that provides comprehensive warehouse management and device lifecycle operations. It enables operators, manufacturers, and service providers to efficiently manage devices through provisioning, maintenance, refurbishment, and end-of-life scenarios. The plugin serves as the primary interface for factory reset operations, device state validation, and warehouse mode management in production and support environments.

## Key Features

### 1. Multi-Mode Factory Reset
- **Full Factory Reset**: Complete device restoration to factory state with all user data and settings removed
- **Light Reset**: User data cleanup while preserving system configurations and applications
- **Internal Reset**: Secure administrative reset with passphrase protection for authorized personnel
- **Customer Factory Reset**: User-initiated reset maintaining operator configurations
- **Cold Factory Reset**: Deep reset including bootloader and system partitions

### 2. Device State Verification
- **Cleanliness Validation**: Verify device is free of residual user data and configurations
- **Age-Based Checking**: Identify files newer than specified time threshold
- **Comprehensive Scanning**: Check multiple partitions and critical directories
- **Detailed Reporting**: Return list of files preventing clean state certification

### 3. Hardware Testing Integration
- **On-Demand Test Execution**: Trigger hardware validation tests remotely
- **Results Retrieval**: Query detailed hardware test results and status
- **RFC-Based Control**: Dynamic test enablement through Remote Feature Control
- **Automated Validation**: Pre-shipment and post-repair verification

### 4. Operational Flexibility
- **Reboot Suppression**: Option to defer reboot for batch operations
- **Asynchronous Operations**: Non-blocking reset execution with event notifications
- **Front Panel Feedback**: Visual status indication via LED displays
- **Multiple Reset Scenarios**: Support for various warehouse workflows

## Primary Use Cases

### Manufacturing and Provisioning
**Scenario**: Device first-time setup and quality assurance
- Execute hardware tests to validate device functionality
- Verify clean state before initial provisioning
- Perform warehouse reset to prepare for customer setup
- Ensure compliance with manufacturing standards

**Benefits**:
- Automated quality control reducing manual inspection time
- Consistent device state across production batches
- Traceability of device validation status

### Return and Refurbishment
**Scenario**: Customer returns and device refurbishment
- Light reset to remove customer data while preserving diagnostics
- Validate complete data removal for privacy compliance
- Re-certify device hardware through testing
- Restore to known-good factory state for resale

**Benefits**:
- GDPR/privacy regulation compliance
- Reduced refurbishment time and costs
- Guaranteed customer data protection
- Device remarketing readiness

### Technical Support and Maintenance
**Scenario**: Remote troubleshooting and device recovery
- Customer factory reset for user-resolvable issues
- Internal reset for advanced troubleshooting by support staff
- State verification before and after support interventions
- Coordinated reset operations across device fleets

**Benefits**:
- Reduced truck rolls and on-site visits
- Remote resolution of software-related issues
- Consistent support procedures across service centers
- Audit trail for support operations

### Warehouse Operations
**Scenario**: Large-scale device management in warehouse facilities
- Batch processing of returned devices
- Automated device testing workflows
- Reboot suppression for multi-step operations
- Integration with warehouse management systems

**Benefits**:
- High-throughput device processing
- Minimal manual intervention required
- Orchestrated workflows with other warehouse systems
- Scalable to thousands of devices

## API Capabilities

### Reset Operations
```json
// Full factory reset with optional reboot control
resetDevice(suppressReboot: bool, resetType: string)
→ Returns: {success: bool, error: string, rebootImmediately: bool}

// Secure internal reset for authorized personnel
internalReset(passPhrase: string)
→ Returns: {success: bool, error: string}

// Light user data reset
lightReset()
→ Returns: {success: bool, error: string}
```

### Validation and Testing
```json
// Check device cleanliness
isClean(age: int)
→ Returns: {clean: bool, files: string[], success: bool, error: string}

// Execute hardware validation
executeHardwareTest()
→ Returns: {success: bool}

// Retrieve test results
getHardwareTestResults()
→ Returns: {success: bool, results: string}
```

### Event Notifications
```json
// Asynchronous completion notification
onResetDone(success: bool, error: string)
```

## Integration Benefits

### For Device Manufacturers
- Streamlined production line integration
- Automated quality assurance workflows
- Reduced manufacturing defect rates
- Compliance with industry standards (Energy Star, etc.)

### For Service Providers
- Remote device management capabilities
- Reduced operational costs (fewer truck rolls)
- Improved customer satisfaction through faster issue resolution
- Fleet-wide device state management

### For Warehouse Operators
- High-throughput device processing
- Integration with existing warehouse management systems
- Batch operation support
- Audit and compliance reporting

### For End Users (Indirect)
- Privacy protection through complete data removal
- Reliable device reset functionality
- Faster support resolution times
- Confidence in refurbished device purchases

## Performance Characteristics

### Reliability
- **Atomic Operations**: Reset operations are transactional and complete fully or rollback
- **Failure Recovery**: Automatic cleanup on reset failure
- **Persistence**: Reboot flags ensure operations complete across power cycles
- **Uptime**: Non-blocking operations maintain system availability

### Scalability
- **Concurrent Operations**: Supports multiple simultaneous client connections
- **Resource Efficiency**: Minimal CPU and memory footprint (~10MB RAM)
- **Batch Support**: Reboot suppression enables sequential multi-device operations
- **Event-Driven**: Asynchronous notifications scale to many subscribers

### Security
- **Passphrase Protection**: Internal reset requires authentication
- **Complete Data Erasure**: Secure deletion meeting DoD 5220.22-M standards
- **Audit Logging**: All operations logged for compliance
- **Role-Based Access**: Different reset types for different authorization levels

### Execution Time
- **Light Reset**: 30-60 seconds typical
- **Full Factory Reset**: 2-5 minutes depending on storage size
- **IsClean Check**: 5-15 seconds for standard devices
- **Hardware Tests**: 1-10 minutes based on test scope

## Compatibility

### Supported Platforms
- RDK Video devices (STB, Smart TV)
- RDK Broadband devices (RDK-B)
- Custom RDK variants with Thunder framework

### Software Requirements
- Thunder/WPEFramework R4.4+
- IARM Bus infrastructure
- Device Settings (DS) HAL
- System Manager service

### Protocol Support
- JSON-RPC 2.0 over HTTP
- JSON-RPC 2.0 over WebSocket
- COM-RPC for internal service communication

## Deployment Model

### Plugin Configuration
The Warehouse plugin is configured via Thunder plugin configuration with settings for:
- Reset operation timeouts
- Passphrase requirements
- Front panel display options
- Logging verbosity
- Default reset behaviors

### Service Dependencies
The plugin integrates with existing RDK services:
- System Manager for reboot control
- Device Settings for hardware state
- RFC for dynamic feature control
- Front Panel service for user feedback

### Backwards Compatibility
The plugin maintains API compatibility across minor versions, with deprecation notices for major API changes. Legacy reset methods are supported alongside new capabilities.

## Future Roadmap

Planned enhancements include:
- Cloud-based orchestration for remote warehouse operations
- Enhanced hardware test reporting with telemetry integration
- Encrypted backup/restore during reset operations
- Network-based device imaging capabilities
- Integration with predictive maintenance systems
