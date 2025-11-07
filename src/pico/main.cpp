#include "main.h"
#include "build_time.hpp"
#include "generated/default_config.hpp"

/* System. */
#include <stdio.h>
#include <malloc.h>

/* FreeRTOS. */
#include "FreeRTOS.h"
#include "croutine.h"
#include "task.h"

/* pico */
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"

/* Vendor. */
#include "etl/list.h"
#include "etl/error_handler.h"
#include "etl/exception.h"

/* GaTas. */
#include "ace/serialadsb.hpp"
#include "ace/dump1090client.hpp"

#include "ace/messagerouter.hpp"
#include "ace/constants.hpp"
#include "ace/aircrafttracker.hpp"
#include "ace/basemodule.hpp"
#include "ace/config.hpp"
#include "ace/inmemorystore.hpp"
#include "ace/flashstore.hpp"
#include "ace/ubloxm8n.hpp"
#include "ace/L76B.hpp"
#include "ace/adsbdecoder.hpp"
#include "ace/picortc.hpp"
#include "ace/wifiservice.hpp"
#include "ace/webserver.hpp"
#include "ace/gpsdecoder.hpp"
#include "ace/acespi.hpp"
#include "ace/bmp280.hpp"
#include "ace/sx1262.hpp"
#include "ace/radiotunerrx_v2.hpp"
#include "ace/rxdataframequeue.hpp"
#include "ace/radiotunertx_v2.hpp"
#include "ace/flarm2024.hpp"
#include "ace/ogn1.hpp"
#include "ace/adsl.hpp"
#include "ace/gdl90service.hpp"
#include "ace/gdloverudp.hpp"
#include "ace/dataport.hpp"
#include "ace/airconnect.hpp"
#include "ace/gatasconnectudp.hpp"
#include "ace/bluetooth.hpp"
#include "ace/fanetace.hpp"
#include "ace/idle.hpp"

const char *GaTas_buildTime = BUILD_TIMESTAMP;

void etlcpp_receive_error(const etl::exception &e)
{
    printf("ETLCPP error was %s in file %s at line %d\n", e.what(), e.file_name(), e.line_number());
}

uint32_t getTotalHeap(void)
{
    extern char __StackLimit, __bss_end__;
    return &__StackLimit - &__bss_end__;
}

uint32_t getFreeHeap(void)
{
    auto m = mallinfo();
    return getTotalHeap() - m.uordblks;
}

void registerModules()
{
    BaseModule::registerModule(AceSpi::NAME, true);
    BaseModule::registerModule(Bmp280::NAME, true);
    // BaseModule::registerModule(Config::NAME, true); // Uncomment if needed
    BaseModule::registerModule(Gdl90Service::NAME, false);
    BaseModule::registerModule(WifiService::NAME, false);
    BaseModule::registerModule(Webserver::NAME, false);
    BaseModule::registerModule(PicoRtc::NAME, false);
    BaseModule::registerModule(Sx1262::NAMES[0], true);
    BaseModule::registerModule(Sx1262::NAMES[1], true);
    BaseModule::registerModule(RadioTunerTx::NAME, false);
    BaseModule::registerModule(RadioTunerRx::NAME, false);
    BaseModule::registerModule(RxDataFrameQueue::NAME, false);
    BaseModule::registerModule(ADSBDecoder::NAME, false);
    BaseModule::registerModule(Flarm2024::NAME, false);
    BaseModule::registerModule(Ogn1::NAME, false);
    BaseModule::registerModule(ADSL::NAME, false);
    BaseModule::registerModule(GDLoverUDP::NAME, false);
    BaseModule::registerModule(GpsDecoder::NAME, false);
    BaseModule::registerModule(UbloxM8N::NAME, true);
    BaseModule::registerModule(L76B::NAME, true);
    BaseModule::registerModule(SerialADSB::NAME, true);
    BaseModule::registerModule(Dump1090Client::NAME, false);
    BaseModule::registerModule(AircraftTracker::NAME, false);
    BaseModule::registerModule(DataPort::NAME, false);
    BaseModule::registerModule(AirConnect::NAME, false);
    BaseModule::registerModule(GatasConnect::NAME, false);
    BaseModule::registerModule(Bluetooth::NAME, false);
    BaseModule::registerModule(FanetAce::NAME, false);
    BaseModule::registerModule(Idle::NAME, false);

    for (auto a : BaseModule::registeredModules())
    {
        printf("Registered %s\n", a.first.cbegin());
    }
}


__scratch_y("aceSpi_Mem") static uint8_t aceSpi_Mem[sizeof(AceSpi)];
__scratch_y("sx1262_1_Mem") static uint8_t sx1262_1_Mem[sizeof(Sx1262)];
__scratch_y("sx1262_2_Mem") static uint8_t sx1262_2_Mem[sizeof(Sx1262)];
__scratch_y("GpsDecoder_Mem") static uint8_t GpsDecoder_Mem[sizeof(GpsDecoder)];
__scratch_y("GPS_Mem") static uint8_t GPS_Mem[etl::max(sizeof(UbloxM8N), sizeof(L76B)) ];
__scratch_y("DataPort_Mem") static uint8_t DataPort_Mem[sizeof(DataPort)];

BaseModule *loadModule(etl::string_view name, etl::imessage_bus &bus, Configuration &config)
{
    // clang-format off
    if (name == Ogn1::NAME)
        return new Ogn1(bus, config);
    if (name == FanetAce::NAME)
        return new FanetAce(bus, config);
    if (name == ADSL::NAME)
        return new ADSL(bus, config);
    if (name == Flarm2024::NAME)
        return new Flarm2024(bus, config);
    if (name == AirConnect::NAME)
        return new AirConnect(bus, config);
    if (name == GatasConnect::NAME)
        return new GatasConnect(bus, config);
    if (name == Bluetooth::NAME)
        return new Bluetooth(bus, config);
    if (name == DataPort::NAME)
        return new (DataPort_Mem) DataPort(bus, config);
    if (name == AircraftTracker::NAME)
        return new AircraftTracker(bus, config);
    if (name == Dump1090Client::NAME)
        return new Dump1090Client(bus, config);
    if (name == SerialADSB::NAME)
        return new SerialADSB(bus, config);
    if (name == L76B::NAME)
        return new (GPS_Mem) L76B(bus, config);
    if (name == UbloxM8N::NAME)
        return new (GPS_Mem) UbloxM8N(bus, config);
    if (name == GpsDecoder::NAME)
        return new (GpsDecoder_Mem) GpsDecoder(bus, config);
    if (name == GDLoverUDP::NAME)
        return new GDLoverUDP(bus, config);
    if (name == ADSBDecoder::NAME)
        return new ADSBDecoder(bus, config);
    if (name == RadioTunerRx::NAME)
        return new RadioTunerRx(bus, config);
    if (name == RadioTunerTx::NAME)
        return new RadioTunerTx(bus, config);
    if (name == RxDataFrameQueue::NAME)
        return new RxDataFrameQueue(bus, config);
    if (name == Sx1262::NAMES[0])
        return new (sx1262_1_Mem) Sx1262(bus, config, 0);
    if (name == Sx1262::NAMES[1])
        return new (sx1262_2_Mem) Sx1262(bus, config, 1);
    if (name == PicoRtc::NAME)
        return new PicoRtc(bus, config);
    if (name == Webserver::NAME)
        return new Webserver(bus, config);
    if (name == WifiService::NAME)
        return new WifiService(bus, config);
    if (name == Gdl90Service::NAME)
        return new Gdl90Service(bus, config);
    if (name == Bmp280::NAME)
        return new Bmp280(bus, config);
    if (name == AceSpi::NAME)
        return new (aceSpi_Mem) AceSpi(bus, config);
    if (name == Idle::NAME)
        return new Idle(bus, config);
    // if (name == Config::NAME) return new Config(bus, FlashStore, DEFAULT_GATAS_CONFIG); // Uncomment if needed
    // clang-format on

    return nullptr; // Unknown name
}

constexpr size_t VOL_DATA_SIZE = 4096;
uint8_t __uninitialized_ram(store[VOL_DATA_SIZE]);
static InMemoryStore<VOL_DATA_SIZE> volatileStore(store);

// Bluetooth stores bonding information at the last sector
// Flash memory Map
// |--------------|---------------|----------------|-------------------|
// | xxBytes.     | 4096Bytes.    | 4096 Bytes.    | 8192 Bytes.       |
// | Application  | Binary Store  | permanentStore | Bluetooth Bonding |

// Used to store Application Configuration
static FlashStore permanentStore{FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE * 2}; // FLASH_SECTOR_SIZE => 4096 on the PICO
// Used to store runtime information not stored in permanent store, counters, id's etc...
static FlashStore binaryStore{FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE * 3}; // FLASH_SECTOR_SIZE => 4096 on the PICO
__scratch_y("GatasMem_Bus") static GATAS::ThreadSafeBus<25> bus;
static Config config(bus, volatileStore, permanentStore, binaryStore, DEFAULT_GATAS_CONFIG);
volatile static bool loadIndicator = false;
volatile static int8_t ledStatusIndicatorPin = -1;
static void load(const etl::string_view str, etl::imessage_bus &bus, Configuration &config, bool force = false)
{
    if (ledStatusIndicatorPin > -1)
    {
        gpio_put(ledStatusIndicatorPin, loadIndicator);
        loadIndicator = !loadIndicator;
    }

#if GATAS_DEBUG == 1
    struct HeapLogger
    {
        ~HeapLogger() { printf("\nFree: %d\n\n", xPortGetFreeHeapSize()); }
    } heapLogger;
#endif

    auto registeredModules = BaseModule::registeredModules();

    printf("\nLoading %s ... ", str.cbegin());

    if (registeredModules[str].hwCheck && config.pinMap(str).empty())
    {
        BaseModule::setModuleStatus(str, GATAS::PostConstruct::HARDWARE_NOT_CONFIGURED);
        printf("not configured for this device, skipping ");
        return;
    }

    if (!(config.isModuleEnabled(str) || force))
    {
        printf("disabled ");
        return;
    }

    if (!registeredModules.contains(str))
    {
        printf("-> not Found ");
        return;
    }

    auto *client = loadModule(str, bus, config);

    if (!client)
    {
        printf("-> out of memory ");
        return;
    }

    printf("-> PostConstruct() ");
    auto result = client->postConstruct();
    if (result == GATAS::PostConstruct::OK)
    {
        BaseModule::setModuleStatus(str, client);
        printf("-> start() ");
        client->start();
    }
    else
    {
        BaseModule::setModuleStatus(str, result);
        printf("-> Unloading reason [%s] ", postConstructToString(result));
        delete client;
    }
}

static void loadModules(void *arg)
{
    (void)arg;
    BaseModule::setModuleStatus(Configuration::CONFIG, &config);

    load(WifiService::NAME, bus, config, true);
    load(Idle::NAME, bus, config, true);

    WifiService *client = (WifiService *)(config.moduleByName(config, WifiService::NAME));
    if (client != nullptr)
    {
        load(Webserver::NAME, bus, config, true);
        load(Bluetooth::NAME, bus, config, true);
    }
    load(AircraftTracker::NAME, bus, config, true);
    load(AceSpi::NAME, bus, config, true);

    // Hardware timings, GPS and connectivity
    load(PicoRtc::NAME, bus, config, true);
    load(GpsDecoder::NAME, bus, config);
    load(UbloxM8N::NAME, bus, config);
    load(L76B::NAME, bus, config);
    load(Gdl90Service::NAME, bus, config);
    load(GDLoverUDP::NAME, bus, config);
    load(DataPort::NAME, bus, config);
    load(AirConnect::NAME, bus, config);
    load(GatasConnect::NAME, bus, config);
    load(Bmp280::NAME, bus, config);

    for (uint8_t i = 0; i < GATAS_MAX_RADIOS; i++)
    {
        load(Sx1262::NAMES[i], bus, config);
    }
    // Other for these two are currently important to ensure configuration on TX is set before RX
    // see RadioTunerRx::enableDisableDatasources()
    // Data sources
    load(RadioTunerTx::NAME, bus, config);
    load(RadioTunerRx::NAME, bus, config);
    load(RxDataFrameQueue::NAME, bus, config, true);
    load(ADSBDecoder::NAME, bus, config);

    // Protocols
    load(ADSL::NAME, bus, config);
    load(FanetAce::NAME, bus, config);
    load(Flarm2024::NAME, bus, config);
    load(Ogn1::NAME, bus, config);
    load(Dump1090Client::NAME, bus, config);

    // SerialADSB messes up the serial terminal, but it will load beyond this point
    // load(SerialADSB::NAME, bus, config);
    // puts("\033[2J\033[H");

    printf(
        R"=(

         ██████╗ ██████╗ ███████╗███╗   ██╗ █████╗  ██████╗███████╗
        ██╔═══██╗██╔══██╗██╔════╝████╗  ██║██╔══██╗██╔════╝██╔════╝
        ██║   ██║██████╔╝█████╗  ██╔██╗ ██║███████║██║     █████╗
        ██║   ██║██╔═══╝ ██╔══╝  ██║╚██╗██║██╔══██║██║     ██╔══╝
        ╚██████╔╝██║     ███████╗██║ ╚████║██║  ██║╚██████╗███████╗
         ╚═════╝ ╚═╝     ╚══════╝╚═╝  ╚═══╝╚═╝  ╚═╝ ╚═════╝╚══════╝

        GA/TAS Device ID: %lX

        )=",
        static_cast<uint32_t>(config.internalStore()->gatasId));
    gpio_put(ledStatusIndicatorPin, 1);

    vTaskDelete(nullptr);
}

#if configGENERATE_RUN_TIME_STATS == 1
void vDiagnosticsTask(void *pvParameters)
{
    constexpr size_t DIAG_STRING_SIZE = 2048; // Adjust based on your needs
    (void)pvParameters;
    while (true)
    {
        char runTimeStats[DIAG_STRING_SIZE] = {0}; // Buffer for runtime stats
        bool hasRunTimeStats = false;

        // Optional: Get CPU usage stats (needs run time counter)
        vTaskGetRunTimeStats(runTimeStats);
        hasRunTimeStats = true;

        // Clear screen and print header
        puts("\033[2J\033[H");
        puts("Note: DiagTasks will show high due to teh wai times are measured?");
        puts("Task Name        Abs Time  %% Time   State  Priority  Stack Left");
        puts("-----------------------------------------------------------------");

        // Get number of tasks
        UBaseType_t numTasks = uxTaskGetNumberOfTasks();
        TaskStatus_t *taskStatusArray = (TaskStatus_t *)pvPortMalloc(numTasks * sizeof(TaskStatus_t));

        if (taskStatusArray)
        {
            numTasks = uxTaskGetSystemState(taskStatusArray, numTasks, nullptr);

            // Sort tasks alphabetically by name
            qsort(taskStatusArray, numTasks, sizeof(TaskStatus_t), [](const void *a, const void *b)
                  { return strcasecmp(((TaskStatus_t *)a)->pcTaskName, ((TaskStatus_t *)b)->pcTaskName); });

            // Parse CPU time stats if available
            char *line = runTimeStats;
            for (UBaseType_t i = 0; i < numTasks; i++)
            {
                char taskName[configMAX_TASK_NAME_LEN + 1] = {0};
                uint32_t absTime = 0;
                float percentTime = 0.0;

                // If runtime stats are available, extract the correct task data
                if (hasRunTimeStats)
                {
                    sscanf(line, "%s %lu %f", taskName, &absTime, &percentTime);
                    line = strchr(line, '\n'); // Move to the next line
                    if (line)
                        line++; // Skip newline
                }

                // Print sorted task info in a single line
                printf("%-16s %-10lu %-7.1f %-6u %-9lu %-10lu\n",
                       taskStatusArray[i].pcTaskName,
                       absTime,
                       percentTime,
                       taskStatusArray[i].eCurrentState,
                       taskStatusArray[i].uxCurrentPriority,
                       uxTaskGetStackHighWaterMark(taskStatusArray[i].xHandle));
            }
            vPortFree(taskStatusArray);
        }

#if defined(LWIP_DEBUG) || MEMP_OVERFLOW_CHECK || LWIP_STATS_DISPLAY
        puts("\n\nLWiP Status:");
        for (int i = 0; i < MEMP_MAX; i++)
        {
            const struct memp_desc *desc = memp_pools[i];
            if (desc == NULL)
                continue;

            struct stats_mem *stats = desc->stats;
            printf("Pool %-20s | avail: %3u | used: %3u | max: %3u | err: %3u\n",
                   desc->desc,
                   (unsigned int)(stats->avail),
                   (unsigned int)(stats->used),
                   (unsigned int)(stats->max),
                   (unsigned int)(stats->err));
        }
#endif

        ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(5000));
    }
}
#endif

//  WifiService::PostConstruct()...assertion "get_core_num() == async_context_core_num(cyw43_async_context)" failed: file "/opt/pico/pico-sdk/src/rp2_common/pico_cyw43_driver/cyw43_driver.c", line 54, function: cyw43_irq_init

// #if PICO_CYW43_ARCH_DEBUG_ENABLED
// #define CYW43_ARCH_DEBUG(...) printf(__VA_ARGS__)
// #else
// #define CYW43_ARCH_DEBUG(...) ((void)0)
// #endif

void vLaunch(void)
{
    // Bootstrap
    BaseModule::initBase();
    puts("--");

    /*** Turn on LED ASAP to indicate that the device is on */
    ledStatusIndicatorPin = config.valueByPath(26, "port5", "O0");
    gpio_init(ledStatusIndicatorPin);
    gpio_set_dir(ledStatusIndicatorPin, GPIO_OUT);
    gpio_put(ledStatusIndicatorPin, 1); // Turn on LED to indicate booting
    /*** Turn on LED ASAP to indicate that the device is on */

    registerModules();
    BaseModule::setModuleStatus(Configuration::NAME, &config);
    BaseModule::setModuleStatus(Config::NAME, &config);
    config.start();

    // Load all the modules
    TaskHandle_t task;
    xTaskCreate(loadModules, "LoadModulesTask", configMINIMAL_STACK_SIZE + 768, NULL, tskIDLE_PRIORITY, &task);
    vTaskCoreAffinitySet(task, 1);

    // Dump some CPU diagnostics to terminal of all running tasks
#if configGENERATE_RUN_TIME_STATS == 1
    xTaskCreate(vDiagnosticsTask, "DiagTask", configMINIMAL_STACK_SIZE + 1024, nullptr, tskIDLE_PRIORITY, nullptr);
#endif

#if NO_SYS && configUSE_CORE_AFFINITY && configNUMBER_OF_CORES > 1
    // we must bind the main task to one core (well at least while the init is called)
    // (note we only do this in NO_SYS mode, because cyw43_arch_freertos
    // takes care of it otherwise)
    vTaskCoreAffinitySet(task, 1);
#endif
    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

void overflowTest()
{
    uint32_t time = 0x000000001;  // Time overflows
    uint32_t start = 0xfffffff0;  // 'old' time
    uint32_t diff = time - start; // Calculate difference
    if (diff != 17)               // Should be 17 if overflow is handled correctly
    {
        panic("Compiler or CPU does not handle overflow correctly");
    }
}

int main()
{
    bool at200Mhz = false;
    if (set_sys_clock_khz(200000, true))
    {
        at200Mhz = true;
    }
    stdio_init_all();
    overflowTest();

    // Load config very eurly in the process so it might be easer to recover
    config.postConstruct();

#if GATAS_DEBUG == 1
    etl::error_handler::set_callback<etlcpp_receive_error>();
#endif

    const char *rtos_name;
#if (configNUMBER_OF_CORES > 1)
    rtos_name = "GaTas FreeRTOS SMP";
#else
    rtos_name = "GaTas FreeRTOS";
#endif

#if (configNUMBER_OF_CORES > 1)
    printf("        Starting %s on both cores at %dMHZ:\n\n", rtos_name, at200Mhz ? 200 : 125);
    vLaunch();
#elif (RUN_FREERTOS_ON_CORE == 1)
    printf("        Starting %s on core 1:\n\n", rtos_name);
    multicore_launch_core1(vLaunch);
    while (true)
        ;
#else
    printf("        Starting %s on core 0 %dMHZ:\n\n", rtos_name, at200Mhz ? 200 : 125);
    vLaunch();
#endif

    return 0;
}
