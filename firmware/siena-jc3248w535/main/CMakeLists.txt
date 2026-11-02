# The firmware's own sources plus the SIBO2 slice of the emulator core
# (V30H, ASIC9, Condor, Honda, SSD, audio) — the same files the web build
# compiles for the Siena.
set(CORE "${CMAKE_CURRENT_LIST_DIR}/../../../core")

idf_component_register(
    SRCS
        app_main.cpp
        app_store.cpp
        calendar_sync.cpp
        audio_out.cpp
        ble_keyboard.cpp
        hid_keymap.cpp
        key_player.cpp
        lcd_axs15231b.cpp
        profiler.cpp
        ram_store.cpp
        screen.cpp
        siena_machine.cpp
        sync_service.cpp
        settings_store.cpp
        terminal_keys.cpp
        ui.cpp
        wifi_link.cpp
        "${CORE}/series3c.cpp"
        "${CORE}/series3c_serial_bridge.cpp"
        "${CORE}/psion_asic9.cpp"
        "${CORE}/psion_condor.cpp"
        "${CORE}/psion_honda.cpp"
        "${CORE}/psion_ssd.cpp"
        "${CORE}/sibo_audio.cpp"
        "${CORE}/v30.cpp"
        "${CORE}/v30_ops_mov.cpp"
        "${CORE}/v30_ops_arith.cpp"
        "${CORE}/v30_ops_ctrl.cpp"
        "${CORE}/v30_ops_misc.cpp"
    INCLUDE_DIRS . "${CORE}"
    LDFRAGMENTS linker.lf
    REQUIRES
        bt esp_hid esp_lcd esp_driver_spi esp_driver_ledc esp_driver_i2s
        esp_driver_gpio esp_driver_usb_serial_jtag esp_partition esp_timer nvs_flash
        fatfs esp_driver_sdmmc sdmmc esp_app_format
        esp_wifi esp_netif esp_event esp_http_client mbedtls)

# The core's diagnostic printf()s use %x for uint32_t, which is `long` on
# Xtensa, and a trace-format comment ends in a backslash; both harmless,
# but IDF builds with -Werror=all.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-multichar -Wno-unused-variable -Wno-format -Wno-comment)

# No locking around function-local statics. The ESP32-S3 is built with
# -mdisable-hardware-atomics, so GCC can't test a static's guard inline and
# calls __cxa_guard_acquire (a FreeRTOS mutex) on every access instead. The
# core reads cached trace flags that way on every emulated instruction:
# measured on the board, ~75% of the emulator's time went to the mutex.
# Every static here is first touched from a single task (the emulator's,
# or app_main before the others start), so the guards buy nothing.
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-threadsafe-statics)

# Let the interpreter's opcode switches compile to jump tables. IDF turns
# them off everywhere so code that runs with the flash cache disabled never
# reads a table from flash; this code only runs in the emulator task, which
# is parked whenever the cache is off, and linker.lf puts these objects'
# read-only data in internal RAM with their code. Without tables each
# switch is a ladder of compares, costed on every emulated instruction.
set_source_files_properties(
    "${CORE}/v30.cpp" "${CORE}/v30_ops_mov.cpp" "${CORE}/v30_ops_arith.cpp"
    "${CORE}/v30_ops_ctrl.cpp" "${CORE}/v30_ops_misc.cpp" "${CORE}/psion_asic9.cpp"
    PROPERTIES COMPILE_OPTIONS "-fjump-tables;-ftree-switch-conversion")

# `idf.py flash` also writes the Siena ROM into its partition. Override with
#   idf.py -DSIENA_ROM=/path/to/rom.bin flash
set(SIENA_ROM "${CMAKE_CURRENT_LIST_DIR}/../../../roms/Siena/vine_v4.20f_eng/vine_v4.20f_eng.bin"
    CACHE FILEPATH "Siena ROM image flashed to the siena_rom partition")
esptool_py_flash_to_partition(flash "siena_rom" "${SIENA_ROM}")

# The app store: every installable Siena app from applib/ on one Flash SSD
# pack, built here and flashed into its own partition (tools/
# build_app_store.py). The pack is the Siena's drive A:.
set(APPLIB "${CMAKE_CURRENT_LIST_DIR}/../../../applib")
set(APP_STORE "${CMAKE_BINARY_DIR}/app_store.bin")
add_custom_command(
    OUTPUT "${APP_STORE}"
    COMMAND python3 "${CMAKE_CURRENT_LIST_DIR}/../tools/build_app_store.py" "${APPLIB}" "${APP_STORE}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../tools/build_app_store.py" "${APPLIB}/catalogue.json"
    COMMENT "Building the app store pack"
    VERBATIM)
add_custom_target(app_store DEPENDS "${APP_STORE}")
add_dependencies(${COMPONENT_LIB} app_store)
esptool_py_flash_to_partition(flash "siena_apps" "${APP_STORE}")
