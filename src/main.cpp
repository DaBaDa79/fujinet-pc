// #include <esp_system.h>
// #include <nvs_flash.h>
// #include <esp32/spiram.h>
// #include <esp32/himem.h>

// #include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "debug.h"
#include "bus.h"
#include "device.h"
// #include "keys.h"
// #include "led.h"

#include "fnSystem.h"
#include "fnConfig.h"
#include "fnDummyWiFi.h"
#include "fnFsSD.h"
#include "fnFsSPIFFS.h"

#include "httpService.h"

#include "fnTaskManager.h"
#include "version.h"

#ifdef BLUETOOTH_SUPPORT
#include "fnBluetooth.h"
#endif

// fnSystem is declared and defined in fnSystem.h/cpp
// fnBtManager is declared and defined in fnBluetooth.h/cpp
// fnLedManager is declared and defined in led.h/cpp
// fnKeyManager is declared and defined in keys.h/cpp
// fnHTTPD is declared and defineid in HttpService.h/cpp

// sioFuji theFuji; // moved to fuji.h/.cpp

volatile sig_atomic_t fn_running = 0;

void main_shutdown_handler()
{
    Debug_println("Shutdown handler called");
    // Give devices an opportunity to clean up before rebooting

    SYSTEM_BUS.shutdown();
}

void sighandler(int signum)
{
    fn_running = 0;
}

void print_version()
{
    printf("FujiNet-PC " FN_VERSION_FULL "\n");
    printf(FN_VERSION_DATE "\n");
#if defined(_WIN32)
    printf("Windows\n");
#elif defined(__linux__)
    printf("Linux\n");
#elif defined(__APPLE__)
    printf("macOS\n");
#endif
}

// Initial setup
void main_setup(int argc, char *argv[])
{
    // program arguments
    int opt;
    while ((opt = getopt(argc, argv, "Vu:c:s:")) != -1) {
        switch (opt) {
            case 'V':
                print_version();
                exit(EXIT_SUCCESS);
            case 'u':
                Config.store_general_interface_url(optarg);
                break;
            case 'c':
                Config.store_general_config_path(optarg);
                break;
            case 's':
                Config.store_general_SD_path(optarg);
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-V] [-u URL] [-c config_file] [-s SD_directory]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

#ifdef DEBUG
    // fnUartDebug.begin(DEBUG_SPEED);
    unsigned long startms = fnSystem.millis();
    Debug_print("\n");
    Debug_print("\n");
    Debug_print("--~--~--~--\n");
    Debug_printf("FujiNet %s Started @ %lu\n", fnSystem.get_fujinet_version(), startms);
    Debug_printf("Starting heap: %u\n", fnSystem.get_free_heap_size());
#ifdef ATARI
    Debug_printf("PsramSize %u\n", fnSystem.get_psram_size());
    // Debug_printf("himem phys %u\n", esp_himem_get_phys_size());
    // Debug_printf("himem free %u\n", esp_himem_get_free_size());
    // Debug_printf("himem reserved %u\n", esp_himem_reserved_area_size());

    Debug_printf("himem phys 0\n");
    Debug_printf("himem free 0\n");
    Debug_printf("himem reserved 0\n");
#endif // ATARI
#endif // DEBUG

/*
    // Install a reboot handler
    esp_register_shutdown_handler(main_shutdown_handler);

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        Debug_println("Erasing flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);

    // Enable GPIO Interrupt Service Routine
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
*/

#if defined(_WIN32)
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) 
    {
        Debug_printf("WSAStartup failed: %d\n", result);
        exit(EXIT_FAILURE);
    }
#endif

    atexit(main_shutdown_handler);
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#if defined(_WIN32)
    signal(SIGBREAK, sighandler);
#endif

    fnSystem.check_hardware_ver();
    Debug_printf("Detected Hardware Version: %s\n", fnSystem.get_hardware_ver_str());

    // fnKeyManager.setup();
    // fnLedManager.setup();

    fnSPIFFS.start();
    fnSDFAT.start(Config.get_general_SD_path().c_str());

    // Load our stored configuration
    Config.load();

    // Now that our main service is running, try connecting to WiFi or BlueTooth
    if (Config.get_bt_status())
    {
#ifdef BLUETOOTH_SUPPORT
        // Start SIO2BT mode if we were in it last shutdown
        fnLedManager.set(eLed::LED_BT, true); // BT LED ON
        fnBtManager.start();
#endif
    }
    else if (Config.get_wifi_enabled())
    {
        // Set up the WiFi adapter if enabled in config
        fnWiFi.start();
        // Go ahead and try reconnecting to WiFi
        fnWiFi.connect();
    }

#ifdef BUILD_ATARI
    theFuji.setup(&SIO);
    SIO.addDevice(&theFuji, SIO_DEVICEID_FUJINET); // the FUJINET!

    SIO.addDevice(&apeTime, SIO_DEVICEID_APETIME); // APETime

    // SIO.addDevice(&udpDev, SIO_DEVICEID_MIDI); // UDP/MIDI device

    pcLink.mount(1, Config.get_general_SD_path().c_str()); // mount SD as PCL1:
    SIO.addDevice(&pcLink, SIO_DEVICEID_PCLINK); // PCLink

    // Create a new printer object, setting its output depending on whether we have SD or not
    FileSystem *ptrfs = fnSDFAT.running() ? (FileSystem *)&fnSDFAT : (FileSystem *)&fnSPIFFS;
    sioPrinter::printer_type ptype = Config.get_printer_type(0);
    if (ptype == sioPrinter::printer_type::PRINTER_INVALID)
        ptype = sioPrinter::printer_type::PRINTER_FILE_TRIM;

    Debug_printf("Creating a default printer using %s storage and type %d\n", ptrfs->typestring(), ptype);

    sioPrinter *ptr = new sioPrinter(ptrfs, ptype);
    fnPrinters.set_entry(0, ptr, ptype, Config.get_printer_port(0));

    SIO.addDevice(ptr, SIO_DEVICEID_PRINTER + fnPrinters.get_port(0)); // P:

    sioR = new sioModem(ptrfs, Config.get_modem_sniffer_enabled()); // Config/User selected sniffer enable

    SIO.addDevice(sioR, SIO_DEVICEID_RS232); // R:

    // SIO.addDevice(&sioV, SIO_DEVICEID_FN_VOICE); // P3:

    // SIO.addDevice(&sioZ, SIO_DEVICEID_CPM); // (ATR8000 CPM)

    // Go setup SIO
    SIO.setup();
#endif // BUILD_ATARI

#ifdef BUILD_CBM
    // Setup IEC Bus
    theFuji.setup(&IEC);
#endif // BUILD_CBM

#ifdef BUILD_LYNX
    theFuji.setup(&ComLynx);
    ComLynx.setup();
#endif

#ifdef BUILD_RS232
    theFuji.setup(&RS232);
    RS232.setup();
    RS232.addDevice(&theFuji,0x70);
#endif

#ifdef BUILD_ADAM
    theFuji.setup(&AdamNet);
    AdamNet.setup();
    fnSDFAT.create_path("/FujiNet");

    Debug_printf("Adding virtual printer\n");
    FileSystem *ptrfs = fnSDFAT.running() ? (FileSystem *)&fnSDFAT : (FileSystem *)&fnSPIFFS;
    adamPrinter::printer_type printer = Config.get_printer_type(0);
    adamPrinter *ptr = new adamPrinter(ptrfs, printer);
    fnPrinters.set_entry(0, ptr, printer, 0);
    AdamNet.addDevice(ptr, ADAMNET_DEVICE_ID_PRINTER);

    if (Config.get_printer_enabled())
        AdamNet.enableDevice(ADAMNET_DEVICE_ID_PRINTER);
    else
        AdamNet.disableDevice(ADAMNET_DEVICE_ID_PRINTER);

#ifdef VIRTUAL_ADAM_DEVICES
    Debug_printf("Physical Device Scanning...\n");
    sioQ = new adamQueryDevice();

#ifndef NO_VIRTUAL_KEYBOARD
    exists = sioQ->adamDeviceExists(ADAMNET_DEVICE_ID_KEYBOARD);
    if (!exists)
    {
        Debug_printf("Adding virtual keyboard\n");
        sioK = new adamKeyboard();
        AdamNet.addDevice(sioK, ADAMNET_DEVICE_ID_KEYBOARD);
    }
    else
        Debug_printf("Physical keyboard found\n");
#endif // NO_VIRTUAL_KEYBOARD

#endif // VIRTUAL_ADAM_DEVICES

#endif // BUILD_ADAM

#ifdef BUILD_APPLE
    // spDevice spsd;
    appleModem *sioR;
    FileSystem *ptrfs = fnSDFAT.running() ? (FileSystem *)&fnSDFAT : (FileSystem *)&fnSPIFFS;
    sioR = new appleModem(ptrfs, Config.get_modem_sniffer_enabled());

    // IWM.addDevice(&theFuji, iwm_fujinet_type_t::FujiNet);
    theFuji.setup(&IWM);
    IWM.setup(); // save device unit SP address somewhere and restore it after reboot?

#endif /* BUILD_APPLE */

    fn_running = 1;

#ifdef DEBUG
    unsigned long endms = fnSystem.millis();
    Debug_printf("Available heap: %u\n", fnSystem.get_free_heap_size());
    Debug_printf("Setup complete @ %lu (%lums)\n", endms, endms - startms);
#endif
}

#ifdef BUILD_S100

// theFuji.setup(&s100Bus);
// SYSTEM_BUS.setup();

#endif /* BUILD_S100*/

// Main high-priority service loop
void fn_service_loop(void *param)
{
    while (fn_running)
    {
        // We don't have any delays in this loop, so IDLE threads will be starved
        // Shouldn't be a problem, but something to keep in mind...

        // Go service BT if it's active
#ifdef BLUETOOTH_SUPPORT
        if (fnBtManager.isActive())
            fnBtManager.service();
        else
    #endif
        SYSTEM_BUS.service();

        fnHTTPD.service();

        taskMgr.service();

        if (fnSystem.check_deferred_reboot())
        {
            // stop the web server first
            // web server is tested by script in restart.html to check if the program is running again
            fnHTTPD.stop();
            // exit the program with special exit code (75)
            // indicate to the controlling script (run-fujinet) that this program (fujinet) should be started again
            fnSystem.reboot(); // calls exit(75)
        }
    }
}

/*
 * This is the start/entry point for an ESP-IDF program (must use "C" linkage)
 */
/*
extern "C"
{
    void app_main()
    {
        // cppcheck-suppress "unusedFunction"
        // Call our setup routine
        main_setup();

// Create a new high-priority task to handle the main loop
// This is assigned to CPU1; the WiFi task ends up on CPU0
#define MAIN_STACKSIZE 32768
#define MAIN_PRIORITY 20
#define MAIN_CPUAFFINITY 1
        xTaskCreatePinnedToCore(fn_service_loop, "fnLoop",
                                MAIN_STACKSIZE, nullptr, MAIN_PRIORITY, nullptr, MAIN_CPUAFFINITY);

        // Now that our main service is running, try connecting to WiFi or BlueTooth
        if (Config.get_bt_status())
        {
#ifdef BLUETOOTH_SUPPORT
            // Start SIO2BT mode if we were in it last shutdown
            fnLedManager.set(eLed::LED_BT, true); // BT LED ON
            fnBtManager.start();
#endif
        }
        else if (Config.get_wifi_enabled())
        {
            // Set up the WiFi adapter if enabled in config
            fnWiFi.start();
            // Go ahead and try reconnecting to WiFi
            fnWiFi.connect();
        }

        // Sit here twiddling our thumbs
        while (true)
            vTaskDelay(9000 / portTICK_PERIOD_MS);
    }
}
*/

int main(int argc, char *argv[])
{
    // Call our setup routine
    main_setup(argc, argv);
    // Enter service loop
    fn_service_loop(nullptr);
    return EXIT_SUCCESS;
}
