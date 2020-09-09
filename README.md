# ModbusTest
Test RS486 interface of Azure Sphere adapter with Modbus RTU protocol

# Flags
Toggle TX_ENABLE flag definition in CMakeLists.txt to compile for HW with or w/o tx enable pin

# Build
Support build with Visual Studio with Visual Studio Extension for Azure Sphere
Support build with Visual Studio Code with Azure Sphere extension

# Test
Update below definition in main.c to align with your test modbus device

'''
#define BAUD_RATE    19200
#define TIMEOUT_MS   1000
#define SLAVE_ID     1
#define REGISTER_ADDR 1840
#define REGISTER_COUNT 100
'''

App LED and network LED will flash RED to as Tx, Rx
Raw PDU package will be print out in debug console




