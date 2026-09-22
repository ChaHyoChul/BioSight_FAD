cmake_minimum_required(VERSION 4.0)
project(BioSightFAD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FAD_SOURCES
    Firmware/src/Application.cpp
    Firmware/src/BuiltInLED.cpp
    Firmware/src/ChecksumCalculator.cpp
    Firmware/src/ControlModeManager.cpp
    Firmware/src/ExternalFanControl.cpp
    Firmware/src/Global.cpp
    Firmware/src/IOManager.cpp
    Firmware/src/KiSANModule.cpp
    Firmware/src/KiSAN_KM6015.cpp
    Firmware/src/KiSAN_KM6023.cpp
    Firmware/src/KiSAN_KM6063.cpp
    Firmware/src/PairedIOControlManager.cpp
    Firmware/src/RemoteServer.cpp
    Firmware/src/SerialPort.cpp
    Firmware/src/TextFormat.cpp
    Firmware/src/Timer.cpp
)

set(FAD_AVR_SOURCES
    Firmware/src/Board.cpp
    Firmware/src/Main.cpp
)

set(FAD_HOST_TEST_SOURCES
    Tests/HostBoard.cpp
    Tests/HostTests.cpp
)

set(FAD_WARNINGS -Wall -Wextra -Wshadow -Wdouble-promotion -Werror)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(FAD_MCU atmega2560)
    set(FAD_CPU_FREQUENCY 16000000UL)

    add_executable(FAD ${FAD_SOURCES} ${FAD_AVR_SOURCES})
    target_include_directories(FAD PRIVATE Firmware/include)
    target_compile_definitions(FAD PRIVATE F_CPU=${FAD_CPU_FREQUENCY})
    target_compile_options(FAD PRIVATE
        -mmcu=${FAD_MCU} -mrelax -funsigned-char -fshort-enums -ffunction-sections -fdata-sections
        -fno-exceptions -fno-rtti -fno-threadsafe-statics ${FAD_WARNINGS}
        $<$<CONFIG:Release>:-O2 -g -flto=auto>
        $<$<CONFIG:Debug>:-Og -g3>)
    target_link_options(FAD PRIVATE
        -mmcu=${FAD_MCU} -mrelax -Wl,--gc-sections -Wl,-Map=$<TARGET_FILE_DIR:FAD>/FAD.map
        $<$<CONFIG:Release>:-O2 -g -flto=auto>)

    add_custom_command(TARGET FAD POST_BUILD
        COMMAND ${AVR_OBJCOPY} -O ihex -R .eeprom $<TARGET_FILE:FAD> $<TARGET_FILE_DIR:FAD>/FAD.hex
        COMMAND ${AVR_OBJCOPY} -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0
                --no-change-warnings $<TARGET_FILE:FAD> $<TARGET_FILE_DIR:FAD>/FAD.eep
        COMMAND ${AVR_SIZE} $<TARGET_FILE:FAD>
        VERBATIM)
else()
    add_executable(HostTests ${FAD_SOURCES} ${FAD_HOST_TEST_SOURCES})
    target_include_directories(HostTests PRIVATE Firmware/include Tests)
    target_compile_definitions(HostTests PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(HostTests PRIVATE -funsigned-char ${FAD_WARNINGS})
endif()
