// emsDevices.h
// transferred from EMS-ESP - https://github.com/emsesp/EMS-ESP
// $Id: emsDevices.h 55 2021-08-29 17:27:39Z juh $

enum Brand {
	    NO_BRAND = 0, // 0
	    BOSCH,        // 1
	    JUNKERS,      // 2
	    BUDERUS,      // 3
	    NEFIT,        // 4
	    SIEGER,       // 5
	    WORCESTER     // 11
};

enum DeviceType {
		 SYSTEM = 0,   // this is us (EMS-ESP)
		 DALLASSENSOR, // for internal dallas sensors
		 BOILER,
		 THERMOSTAT,
		 MIXER,
		 SOLAR,
		 HEATPUMP,
		 GATEWAY,
		 SWITCH,
		 CONTROLLER,
		 CONNECT,
		 GENERIC,
		 UNKNOWN
};

// static device IDs
#define EMS_DEVICE_ID_BOILER   0x08 // fixed device_id for Master Boiler/UBA
#define EMS_DEVICE_ID_BOILER_1  0x70 // fixed device_id for 1st. Cascade Boiler/UBA
#define EMS_DEVICE_ID_BOILER_F  0x7F // fixed device_id for last Cascade Boiler/UBA

// generic type IDs
#define EMS_TYPE_VERSION     0x02 // type ID for Version information. Generic across all EMS devices.
#define EMS_TYPE_UBADevices  0x07 // EMS connected devices

// device flags: The lower 4 bits hold the unique identifier, the upper 4 bits are used for specific flags
#define EMS_DEVICE_FLAG_NONE  0

// Boiler
#define EMS_DEVICE_FLAG_EMS       1
#define EMS_DEVICE_FLAG_EMSPLUS   2
#define EMS_DEVICE_FLAG_HT3       3
#define EMS_DEVICE_FLAG_HEATPUMP  4

// Solar Module
#define EMS_DEVICE_FLAG_SM10   1
#define EMS_DEVICE_FLAG_SM100  2
#define EMS_DEVICE_FLAG_ISM    3

// Mixer Module
#define EMS_DEVICE_FLAG_MMPLUS  1
#define EMS_DEVICE_FLAG_MM10    2
#define EMS_DEVICE_FLAG_IPM     3

// Thermostats
#define EMS_DEVICE_FLAG_NO_WRITE     (1 << 7) // last bit
#define EMS_DEVICE_FLAG_JUNKERS_OLD  (1 << 6) // 6th bit set if older models, like FR120, FR100
#define EMS_DEVICE_FLAG_EASY         1
#define EMS_DEVICE_FLAG_RC10         2
#define EMS_DEVICE_FLAG_RC20         3
#define EMS_DEVICE_FLAG_RC20_N       4 // Variation on RC20, Older, like ES72
#define EMS_DEVICE_FLAG_RC30_N       5 // variation on RC30, Newer models
#define EMS_DEVICE_FLAG_RC30         6
#define EMS_DEVICE_FLAG_RC35         7
#define EMS_DEVICE_FLAG_RC300        8
#define EMS_DEVICE_FLAG_RC100        9
#define EMS_DEVICE_FLAG_JUNKERS      10
#define EMS_DEVICE_FLAG_CRF          11 // CRF200 only monitor


// Boilers - 0x08

typedef struct {
    int code;
    int deviceType;
    char name[100];
    int flags;
} emsDevices;

emsDevices emsDev[] =
    {
     { 64, BOILER, "BK13/BK15/Smartline/GB1x2", EMS_DEVICE_FLAG_NONE},
     { 72, BOILER, "GB125/MC10", EMS_DEVICE_FLAG_EMS},
     { 84, BOILER, "Logamax Plus GB022", EMS_DEVICE_FLAG_NONE},
     { 95, BOILER, "Condens 2500/Logamax/Logomatic/Cerapur Top/Greenstar/Generic HT3", EMS_DEVICE_FLAG_HT3},
     {115, BOILER, "Topline/GB162", EMS_DEVICE_FLAG_NONE},
     {122, BOILER, "Proline", EMS_DEVICE_FLAG_NONE},
     {123, BOILER, "GBx72/Trendline/Cerapur/Greenstar Si/27i", EMS_DEVICE_FLAG_NONE},
     {131, BOILER, "GB212", EMS_DEVICE_FLAG_NONE},
     {133, BOILER, "Logano GB125/KB195i/Logamatic MC110", EMS_DEVICE_FLAG_NONE},
     {167, BOILER, "Cerapur Aero", EMS_DEVICE_FLAG_NONE},
     {170, BOILER, "Logano GB212", EMS_DEVICE_FLAG_NONE},
     {172, BOILER, "Enviline/Compress 6000AW/Hybrid 7000iAW/SupraEco", EMS_DEVICE_FLAG_HEATPUMP},
     {195, BOILER, "Condens 5000i/Greenstar 8000", EMS_DEVICE_FLAG_NONE},
     {203, BOILER, "Logamax U122/Cerapur", EMS_DEVICE_FLAG_NONE},
     {208, BOILER, "Logamax Plus/GB192/Condens GC9000", EMS_DEVICE_FLAG_NONE},
     {210, BOILER, "Cascade MC400", EMS_DEVICE_FLAG_NONE},
     {211, BOILER, "EasyControl Adapter", EMS_DEVICE_FLAG_NONE},
     {234, BOILER, "Logamax Plus GB122", EMS_DEVICE_FLAG_NONE},
     {206, BOILER, "Ecomline Excellent", EMS_DEVICE_FLAG_NONE},
     
     // Controllers - 0x09 / 0x10 / 0x50
     { 68, CONTROLLER, "BC10/RFM20", EMS_DEVICE_FLAG_NONE}, // 0x09
     { 84, CONTROLLER, "GB022", EMS_DEVICE_FLAG_NONE},
     { 89, CONTROLLER, "BC10 GB142", EMS_DEVICE_FLAG_NONE}, // 0x09
     { 95, CONTROLLER, "HT3", EMS_DEVICE_FLAG_NONE}, // 0x09
     {114, CONTROLLER, "BC10", EMS_DEVICE_FLAG_NONE}, // 0x09
     {125, CONTROLLER, "BC25", EMS_DEVICE_FLAG_NONE}, // 0x09
     {152, CONTROLLER, "Controller", EMS_DEVICE_FLAG_NONE}, // 0x09
     {169, CONTROLLER, "BC40", EMS_DEVICE_FLAG_NONE}, // 0x09
     {190, CONTROLLER, "BC10", EMS_DEVICE_FLAG_NONE}, // 0x09
     {194, CONTROLLER, "BC10", EMS_DEVICE_FLAG_NONE}, // 0x09
     {206, CONTROLLER, "Ecomline", EMS_DEVICE_FLAG_NONE}, // 0x09
     {207, CONTROLLER, "Sense II/CS200", EMS_DEVICE_FLAG_NONE}, // 0x10
     {209, CONTROLLER, "ErP", EMS_DEVICE_FLAG_NONE}, // 0x09
     {218, CONTROLLER, "M200/RFM200", EMS_DEVICE_FLAG_NONE}, // 0x50
     {224, CONTROLLER, "9000i", EMS_DEVICE_FLAG_NONE}, // 0x09
     {230, CONTROLLER, "BC Base", EMS_DEVICE_FLAG_NONE}, // 0x09
     {241, CONTROLLER, "Condens 5000i", EMS_DEVICE_FLAG_NONE}, // 0x09
     
     // Thermostat - not currently supporting write operations, like the Easy/100 types - 0x18
     {202, THERMOSTAT, "Logamatic TC100/Moduline Easy", EMS_DEVICE_FLAG_EASY | EMS_DEVICE_FLAG_NO_WRITE}, // 0x18, cannot write
     {203, THERMOSTAT, "EasyControl CT200", EMS_DEVICE_FLAG_EASY | EMS_DEVICE_FLAG_NO_WRITE}, // 0x18, cannot write
     
     // Thermostat - Buderus/Nefit/Bosch specific - 0x17 / 0x10 / 0x18 / 0x19 / 0x38
     { 67, THERMOSTAT, "RC30", EMS_DEVICE_FLAG_RC30_N},// 0x10 - based on RC35
     { 77, THERMOSTAT, "RC20/Moduline 300", EMS_DEVICE_FLAG_RC20},// 0x17
     { 78, THERMOSTAT, "Moduline 400", EMS_DEVICE_FLAG_RC30}, // 0x10
     { 79, THERMOSTAT, "RC10/Moduline 100", EMS_DEVICE_FLAG_RC10},// 0x17
     { 80, THERMOSTAT, "Moduline 200", EMS_DEVICE_FLAG_RC10}, // 0x17
     { 86, THERMOSTAT, "RC35", EMS_DEVICE_FLAG_RC35}, // 0x10
     { 90, THERMOSTAT, "RC10/Moduline 100", EMS_DEVICE_FLAG_RC20_N}, // 0x17
     { 93, THERMOSTAT, "RC20RF", EMS_DEVICE_FLAG_RC20}, // 0x19
     { 94, THERMOSTAT, "RFM20 Remote", EMS_DEVICE_FLAG_NONE}, // 0x18
     {157, THERMOSTAT, "RC200/CW100", EMS_DEVICE_FLAG_RC100}, // 0x18
     {158, THERMOSTAT, "RC300/RC310/Moduline 3000/1010H/CW400/Sense II", EMS_DEVICE_FLAG_RC300}, // 0x10
     {165, THERMOSTAT, "RC100/Moduline 1000/1010", EMS_DEVICE_FLAG_RC100}, // 0x18, 0x38
     {216, THERMOSTAT, "CRF200S", EMS_DEVICE_FLAG_CRF | EMS_DEVICE_FLAG_NO_WRITE}, // 0x18
     
     // Thermostat - Sieger - 0x10 / 0x17
     { 66, THERMOSTAT, "ES72/RC20", EMS_DEVICE_FLAG_RC20_N}, // 0x17 or remote
     { 76, THERMOSTAT, "ES73", EMS_DEVICE_FLAG_RC35}, // 0x10
     {113, THERMOSTAT, "ES72/RC20", EMS_DEVICE_FLAG_RC20_N}, // 0x17
     
     // Thermostat - Junkers - 0x10
     {105, THERMOSTAT, "FW100", EMS_DEVICE_FLAG_JUNKERS},
     {106, THERMOSTAT, "FW200", EMS_DEVICE_FLAG_JUNKERS},
     {107, THERMOSTAT, "FR100", EMS_DEVICE_FLAG_JUNKERS | EMS_DEVICE_FLAG_JUNKERS_OLD}, // older model
     {108, THERMOSTAT, "FR110", EMS_DEVICE_FLAG_JUNKERS | EMS_DEVICE_FLAG_JUNKERS_OLD}, // older model
     {111, THERMOSTAT, "FR10", EMS_DEVICE_FLAG_JUNKERS},  
     {147, THERMOSTAT, "FR50", EMS_DEVICE_FLAG_JUNKERS | EMS_DEVICE_FLAG_JUNKERS_OLD},
     {191, THERMOSTAT, "FR120", EMS_DEVICE_FLAG_JUNKERS | EMS_DEVICE_FLAG_JUNKERS_OLD}, // older model
     {192, THERMOSTAT, "FW120", EMS_DEVICE_FLAG_JUNKERS},
     
     // Solar Modules - 0x30, 0x2A (for ww)
     { 73, SOLAR, "SM10", EMS_DEVICE_FLAG_SM10},
     {101, SOLAR, "ISM1", EMS_DEVICE_FLAG_ISM},
     {162, SOLAR, "SM50", EMS_DEVICE_FLAG_SM100},
     {163, SOLAR, "SM100/MS100", EMS_DEVICE_FLAG_SM100},
     {164, SOLAR, "SM200/MS200", EMS_DEVICE_FLAG_SM100},
     
     // Mixer Modules - 0x20-0x27 for HC, 0x28-0x29 for WWC
     { 69, MIXER, "MM10", EMS_DEVICE_FLAG_MM10},
     {102, MIXER, "IPM", EMS_DEVICE_FLAG_IPM},
     {159, MIXER, "MM50", EMS_DEVICE_FLAG_MMPLUS},
     {160, MIXER, "MM100", EMS_DEVICE_FLAG_MMPLUS},
     {161, MIXER, "MM200", EMS_DEVICE_FLAG_MMPLUS},
     
     // Heat Pumps - 0x38
     {200, HEATPUMP, "HP Module", EMS_DEVICE_FLAG_NONE},
     {252, HEATPUMP, "HP Module", EMS_DEVICE_FLAG_NONE},
     
     // Connect devices - 0x02
     {171, CONNECT, "OpenTherm Converter", EMS_DEVICE_FLAG_NONE},
     {205, CONNECT, "Moduline Easy Connect", EMS_DEVICE_FLAG_NONE},
     {206, CONNECT, "Easy Connect", EMS_DEVICE_FLAG_NONE},
     
     // Switches - 0x11
     { 71, SWITCH, "WM10", EMS_DEVICE_FLAG_NONE},
     
     // Gateways - 0x48
     {189, GATEWAY, "KM200/MB LAN 2", EMS_DEVICE_FLAG_NONE}
};

// the following are from Th3M3/buderus_ems-wiki
#define EMS_TYPE_UBAErrorMessage 0xbf
#define EMS_TYPE_UBAOutdoorTempMessage 0xd1
#define EMS_TYPE_UBAMonitorFast 0xe4
#define EMS_TYPE_UBAMonitorSlow 0xe5
#define EMS_TYPE_UBAMonitorWater 0xe9


// these are from emsesp/EMS-ESP
#define EMS_TYPE_UBASettingsWW 0x26
#define EMS_TYPE_UBAParameterWW 0x33
#define EMS_TYPE_UBAFunctionTest 0x1D
#define EMS_TYPE_UBAFlags 0x35
#define EMS_TYPE_UBASetPoints 0x1A
#define EMS_TYPE_UBAParameters 0x16
#define EMS_TYPE_UBAParametersPlus 0xE6
#define EMS_TYPE_UBAParameterWWPlus 0xEA
#define EMS_TYPE_UBAInformation 0x495
#define EMS_TYPE_UBAEnergySupplied 0x494

#define EMS_BOILER_SELFLOWTEMP_HEATING 20 // was originally 70, changed to 30 for issue #193, then to 20 with issue #344

// Generic Types
#define EMS_TYPE_RCTime 0x06 // time
#define EMS_TYPE_RCOutdoorTemp 0xA3 // is an automatic thermostat broadcast, outdoor external temp

// Type offsets
#define EMS_OFFSET_RC10StatusMessage_setpoint 1 // setpoint temp
#define EMS_OFFSET_RC10StatusMessage_curr 2 // current temp
#define EMS_OFFSET_RC10Set_temp 4 // position of thermostat setpoint temperature

#define EMS_OFFSET_RC20StatusMessage_setpoint 1  // setpoint temp
#define EMS_OFFSET_RC20StatusMessage_curr 2  // current temp
#define EMS_OFFSET_RC20Set_mode 23 // position of thermostat mode
#define EMS_OFFSET_RC20Set_temp 28 // position of thermostat setpoint temperature

#define EMS_OFFSET_RC20_2_Set_mode 3 // ES72 - see https://github.com/emsesp/EMS-ESP/issues/334
#define EMS_OFFSET_RC20_2_Set_temp_night 1 // ES72
#define EMS_OFFSET_RC20_2_Set_temp_day 2 // ES72

#define EMS_OFFSET_RC30StatusMessage_setpoint 1  // setpoint temp
#define EMS_OFFSET_RC30StatusMessage_curr 2  // current temp
#define EMS_OFFSET_RC30Set_mode 23 // position of thermostat mode
#define EMS_OFFSET_RC30Set_temp 28 // position of thermostat setpoint temperature

#define EMS_OFFSET_RC35StatusMessage_setpoint 2  // desired temp
#define EMS_OFFSET_RC35StatusMessage_curr 3  // current temp
#define EMS_OFFSET_RC35StatusMessage_mode 1  // day mode, also summer on RC3's
#define EMS_OFFSET_RC35StatusMessage_mode1 0  // for holiday mode
#define EMS_OFFSET_RC35Set_mode 7  // position of thermostat mode
#define EMS_OFFSET_RC35Set_temp_day 2  // position of thermostat setpoint temperature for day time
#define EMS_OFFSET_RC35Set_temp_night 1  // position of thermostat setpoint temperature for night time
#define EMS_OFFSET_RC35Set_temp_holiday 3  // temp during holiday mode
#define EMS_OFFSET_RC35Set_heatingtype 0  // e.g. floor heating = 3
#define EMS_OFFSET_RC35Set_targetflowtemp 14 // target flow temperature
#define EMS_OFFSET_RC35Set_seltemp 37 // selected temp
#define EMS_OFFSET_RC35Set_noreducetemp 38 // temp to stop reducing
#define EMS_OFFSET_RC35Set_temp_offset 6
#define EMS_OFFSET_RC35Set_temp_flowoffset 24
#define EMS_OFFSET_RC35Set_temp_design 17
#define EMS_OFFSET_RC35Set_temp_design_floor 36
#define EMS_OFFSET_RC35Set_temp_summer 22
#define EMS_OFFSET_RC35Set_temp_nofrost 23

#define EMS_OFFSET_EasyStatusMessage_setpoint 10 // setpoint temp
#define EMS_OFFSET_EasyStatusMessage_curr 8  // current temp

#define EMS_OFFSET_RCPLUSStatusMessage_mode 10 // thermostat mode (auto, manual)
#define EMS_OFFSET_RCPLUSStatusMessage_setpoint 3  // setpoint temp
#define EMS_OFFSET_RCPLUSStatusMessage_curr 0  // current temp
#define EMS_OFFSET_RCPLUSStatusMessage_currsetpoint 6  // target setpoint temp
#define EMS_OFFSET_RCPLUSSet_mode 0  // operation mode(Auto=0xFF, Manual=0x00)
#define EMS_OFFSET_RCPLUSSet_temp_comfort3 1  // comfort3 level
#define EMS_OFFSET_RCPLUSSet_temp_comfort2 2  // comfort2 level
#define EMS_OFFSET_RCPLUSSet_temp_comfort1 3  // comfort1 level
#define EMS_OFFSET_RCPLUSSet_temp_eco 4  // eco level
#define EMS_OFFSET_RCPLUSSet_temp_setpoint 8  // temp setpoint, when changing of templevel (in auto) value is reset to FF
#define EMS_OFFSET_RCPLUSSet_manual_setpoint 10 // manual setpoint

#define EMS_OFFSET_JunkersStatusMessage_daymode 0  // 3 = day, 2 = night, 1 = nofrost
#define EMS_OFFSET_JunkersStatusMessage_mode 1  // current mode, 1 = manual, 2 = auto
#define EMS_OFFSET_JunkersStatusMessage_setpoint 2  // setpoint temp
#define EMS_OFFSET_JunkersStatusMessage_curr 4  // current temp
#define EMS_OFFSET_JunkersSetMessage_day_temp 17 // EMS offset to set temperature on thermostat for day mode
#define EMS_OFFSET_JunkersSetMessage_night_temp 16 // EMS offset to set temperature on thermostat for night mode
#define EMS_OFFSET_JunkersSetMessage_no_frost_temp 15 // EMS offset to set temperature on thermostat for no frost mode
#define EMS_OFFSET_JunkersSetMessage_set_mode 14 // EMS offset to set mode on thermostat
#define EMS_OFFSET_JunkersSetMessage2_set_mode 4  // EMS offset to set mode on thermostat
#define EMS_OFFSET_JunkersSetMessage2_no_frost_temp 5
#define EMS_OFFSET_JunkersSetMessage2_eco_temp 6
#define EMS_OFFSET_JunkersSetMessage2_heat_temp 7

#define AUTO_HEATING_CIRCUIT 0

// Installation settings
#define EMS_TYPE_IBASettings 0xA5 // installation settings
#define EMS_TYPE_wwSettings 0x37 // ww settings
#define EMS_TYPE_time 0x06 // time
