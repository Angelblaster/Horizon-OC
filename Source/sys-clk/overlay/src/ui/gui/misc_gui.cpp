/*
 *
 * Copyright (c) Souldbminer, Lightos_ and Horizon OC Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "misc_gui.h"
#include "fatal_gui.h"
#include "../format.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <notification.h>
#include "labels.h"
#if IS_MINIMAL == 1
#pragma message("Compiling with minimal features")
#endif

class RamSubmenuGui;
class RamTimingsSubmenuGui;
class RamLatenciesSubmenuGui;
class CpuSubmenuGui;
class GpuSubmenuGui;
class GpuCustomTableSubmenuGui;

MiscGui::MiscGui()
{
    this->configList = new SysClkConfigValueList {};
}

MiscGui::~MiscGui()
{
    delete this->configList;
    this->configToggles.clear();
    this->configTrackbars.clear();
    this->configButtons.clear();
    this->configRanges.clear();
}

void MiscGui::addConfigToggle(SysClkConfigValue configVal, const char* altName) {
    const char* configName = altName ? altName : sysclkFormatConfigValue(configVal, true);
    tsl::elm::ToggleListItem* toggle = new tsl::elm::ToggleListItem(configName, this->configList->values[configVal]);
    toggle->setStateChangedListener([this, configVal](bool state) {
        this->configList->values[configVal] = uint64_t(state);
        Result rc = sysclkIpcSetConfigValues(this->configList);
        if (R_FAILED(rc))
            FatalGui::openWithResultCode("sysclkIpcSetConfigValues", rc);
        this->lastContextUpdate = armGetSystemTick();
    });
    this->listElement->addItem(toggle);
    this->configToggles[configVal] = toggle;
}


void MiscGui::addConfigButton(SysClkConfigValue configVal,
    const char* altName,
    const ValueRange& range,
    const std::string& categoryName,
    const ValueThresholds* thresholds,
    const std::map<uint32_t, std::string>& labels,
    const std::vector<NamedValue>& namedValues,
    bool showDefaultValue)
{
    const char* configName = altName ? altName : sysclkFormatConfigValue(configVal, true);

    tsl::elm::ListItem* listItem = new tsl::elm::ListItem(configName);

    uint64_t currentValue = this->configList->values[configVal];
    char valueText[32];
    if (currentValue == 0 && showDefaultValue) {
        snprintf(valueText, sizeof(valueText), "%s", VALUE_DEFAULT_TEXT);
    } else {
        bool foundNamedValue = false;
        for (const auto& namedValue : namedValues) {
            if (currentValue == namedValue.value) {
                snprintf(valueText, sizeof(valueText), "%s", namedValue.name.c_str());
                foundNamedValue = true;
                break;
            }
        }

        if (!foundNamedValue) {
            uint64_t displayValue = currentValue / range.divisor;
            if (!range.suffix.empty()) {
                snprintf(valueText, sizeof(valueText), "%lu %s", displayValue, range.suffix.c_str());
            } else {
                snprintf(valueText, sizeof(valueText), "%lu", displayValue);
            }
        }
    }
    listItem->setValue(valueText);

    ValueThresholds thresholdsCopy = (thresholds ? *thresholds : ValueThresholds{});

    listItem->setClickListener(
        [this, configVal, range, categoryName, thresholdsCopy, labels, namedValues, showDefaultValue](u64 keys)
        {
            if ((keys & HidNpadButton_A) == 0)
                return false;

            std::uint32_t currentValue = this->configList->values[configVal];

            if (thresholdsCopy.warning != 0 || thresholdsCopy.danger != 0) {

                tsl::changeTo<ValueChoiceGui>(
                    currentValue,
                    range,
                    categoryName,
                    [this, configVal](std::uint32_t value) {
                        this->configList->values[configVal] = value;
                        Result rc = sysclkIpcSetConfigValues(this->configList);
                        if (R_FAILED(rc)) {
                            FatalGui::openWithResultCode("sysclkIpcSetConfigValues", rc);
                            return false;
                        }
                        this->lastContextUpdate = armGetSystemTick();
                        return true;
                    },
                    thresholdsCopy,
                    true,
                    labels,
                    namedValues,
                    showDefaultValue
                );
            } else {

                tsl::changeTo<ValueChoiceGui>(
                    currentValue,
                    range,
                    categoryName,
                    [this, configVal](std::uint32_t value) {
                        this->configList->values[configVal] = value;
                        Result rc = sysclkIpcSetConfigValues(this->configList);
                        if (R_FAILED(rc)) {
                            FatalGui::openWithResultCode("sysclkIpcSetConfigValues", rc);
                            return false;
                        }
                        this->lastContextUpdate = armGetSystemTick();
                        return true;
                    },
                    ValueThresholds(),
                    false,
                    labels,
                    namedValues,
                    showDefaultValue
                );
            }

            return true;
        });

    this->listElement->addItem(listItem);
    this->configButtons[configVal] = listItem;
    this->configRanges[configVal] = range;
    this->configNamedValues[configVal] = namedValues;
}

void MiscGui::updateConfigToggles() {
    for (const auto& [value, toggle] : this->configToggles) {
        if (toggle != nullptr)
            toggle->setState(this->configList->values[value]);
    }
}

void MiscGui::addFreqButton(SysClkConfigValue configVal,
                            const char* altName,
                            SysClkModule module,
                            const std::map<uint32_t, std::string>& labels)
{
    const char* configName = altName ? altName : sysclkFormatConfigValue(configVal, true);

    tsl::elm::ListItem* listItem = new tsl::elm::ListItem(configName);

    uint64_t currentMHz = this->configList->values[configVal];
    char valueText[32];
    snprintf(valueText, sizeof(valueText), "%lu MHz", currentMHz);
    listItem->setValue(valueText);

    listItem->setClickListener(
        [this, configVal, module, labels](u64 keys)
        {
            if ((keys & HidNpadButton_A) == 0)
                return false;

            std::uint32_t hzList[SYSCLK_FREQ_LIST_MAX];
            std::uint32_t hzCount;

            Result rc = sysclkIpcGetFreqList(module, hzList, SYSCLK_FREQ_LIST_MAX, &hzCount);
            if (R_FAILED(rc)) {
                FatalGui::openWithResultCode("sysclkIpcGetFreqList", rc);
                return false;
            }

            std::uint32_t currentHz = this->configList->values[configVal] * 1'000'000;

            tsl::changeTo<FreqChoiceGui>(
                currentHz,
                hzList,
                hzCount,
                module,
                [this, configVal](std::uint32_t hz)
                {
                    uint64_t mhz = hz / 1'000'000;
                    this->configList->values[configVal] = mhz;

                    Result rc = sysclkIpcSetConfigValues(this->configList);
                    if (R_FAILED(rc)) {
                        FatalGui::openWithResultCode("sysclkIpcSetConfigValues", rc);
                        return false;
                    }

                    this->lastContextUpdate = armGetSystemTick();
                    return true;
                },
                false,
                labels
            );

            return true;
        });

    this->listElement->addItem(listItem);
    this->configButtons[configVal] = listItem;

    this->configRanges[configVal] = ValueRange(0, 0, 0, "MHz", 1);
}

void MiscGui::listUI()
{
    Result rc = sysclkIpcGetConfigValues(configList);
    if (R_FAILED(rc)) [[unlikely]] {
        FatalGui::openWithResultCode("sysclkIpcGetConfigValues", rc);
        return;
    }

    this->listElement->addItem(new tsl::elm::CategoryHeader("Safety Settings"));
    addConfigToggle(HocClkConfigValue_UncappedClocks, nullptr);
    addConfigToggle(HocClkConfigValue_ThermalThrottle, nullptr);
    addConfigToggle(HocClkConfigValue_HandheldTDP, nullptr);
//  addConfigToggle(HocClkConfigValue_EnforceBoardLimit, nullptr);

    #if IS_MINIMAL == 0
        std::map<uint32_t, std::string> labels_pwr_l = {
            {6400, "Official Rating"}
        };

        if(IsHoag()) {
            ValueThresholds tdpThresholdsLite(6400, 7500);
            addConfigButton(
                HocClkConfigValue_LiteTDPLimit,
                "Lite TDP Threshold",
                ValueRange(4000, 8000, 100, "mW", 1),
                "Power",
                &tdpThresholdsLite,
                labels_pwr_l
            );
        } else {
            ValueThresholds tdpThresholds(9600, 11000);
            addConfigButton(
                HocClkConfigValue_HandheldTDPLimit,
                "TDP Threshold",
                ValueRange(8000, 12000, 100, "mW", 1),
                "Power",
                &tdpThresholds
            );
        }

        ValueThresholds throttleThresholds(70, 80);
        addConfigButton(
            HocClkConfigValue_ThermalThrottleThreshold,
            "Thermal Throttle Limit",
            ValueRange(50, 85, 1, "°C", 1),
            "Temp",
            &throttleThresholds
        );
    #endif

    ValueThresholds thresholdsDisabled(0, 0);
    std::vector<NamedValue> noNamedValues = {};

    this->listElement->addItem(new tsl::elm::CategoryHeader("CPU Settings"));
    addConfigToggle(HocClkConfigValue_OverwriteBoostMode, nullptr);
    std::vector<NamedValue> gpuSchedValues = {
        NamedValue("Do not override", GpuSchedulingMode_DoNotOverride),
        NamedValue("Enabled (Default)", GpuSchedulingMode_Enabled, "96.6% limit"),
        NamedValue("Disabled", GpuSchedulingMode_Disabled, "99.7% limit"),
    };

    this->listElement->addItem(new tsl::elm::CategoryHeader("GPU Settings"));
    addConfigButton(
        HorizonOCConfigValue_GPUScheduling,
        "GPU Scheduling Override",
        ValueRange(0, 0, 1, "", 0),
        "GPU Scheduling Override",
        &thresholdsDisabled,
        {},
        gpuSchedValues,
        false
    );

    if (IsMariko()) {
        std::vector<NamedValue> dvfsValues = {
            NamedValue("Disabled", DVFSMode_Disabled),
            NamedValue("PCV Hijack", DVFSMode_Hijack),
            // NamedValue("Official Service", DVFSMode_OfficialService),
            // NamedValue("Hack", DVFSMode_Hack),
        };

        addConfigButton(
            HorizonOCConfigValue_DVFSMode,
            "GPU DVFS Mode",
            ValueRange(0, 0, 1, "", 0),
            "GPU DVFS Mode",
            &thresholdsDisabled,
            {},
            dvfsValues,
            false
        );

        std::vector<NamedValue> dvfsOffset = {
            NamedValue("-50 mV", 0xFFFFFFCE),
            NamedValue("-45 mV", 0xFFFFFFD3),
            NamedValue("-40 mV", 0xFFFFFFD8),
            NamedValue("-30 mV", 0xFFFFFFE2),
            NamedValue("-25 mV", 0xFFFFFFE7),
            NamedValue("-20 mV", 0xFFFFFFEC),
            NamedValue("-10 mV", 0xFFFFFFF6),
            NamedValue(" -5 mV", 0xFFFFFFFB),
            NamedValue("Disabled",          0),
            NamedValue(" +5 mV",          5),
            NamedValue("+10 mV",         10),
            NamedValue("+15 mV",         15),
            NamedValue("+20 mV",         20),
        };

        addConfigButton(HorizonOCConfigValue_DVFSOffset, "GPU DVFS Offset", ValueRange(0, 12, 1, "", 0), "GPU DVFS Offset", &thresholdsDisabled, {}, dvfsOffset, false);
    }

    this->listElement->addItem(new tsl::elm::CategoryHeader("KIP"));

    tsl::elm::ListItem* saveBtn = new tsl::elm::ListItem("Save KIP Settings");
    saveBtn->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            Result rc = hocClkIpcSetKipData();
            if (R_FAILED(rc)) {
                FatalGui::openWithResultCode("hocClkIpcSetKipData", rc);
                return false;
            }
            return true;
        }
        return false;
    });
    this->listElement->addItem(saveBtn);

    tsl::elm::ListItem* ramSubmenu = new tsl::elm::ListItem("RAM Settings");
    ramSubmenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<RamSubmenuGui>();
            return true;
        }
        return false;
    });
    this->listElement->addItem(ramSubmenu);

    tsl::elm::ListItem* cpuSubmenu = new tsl::elm::ListItem("CPU Settings");
    cpuSubmenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<CpuSubmenuGui>();
            return true;
        }
        return false;
    });
    this->listElement->addItem(cpuSubmenu);

    tsl::elm::ListItem* gpuSubmenu = new tsl::elm::ListItem("GPU Settings");
    gpuSubmenu->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<GpuSubmenuGui>();
            return true;
        }
        return false;
    });
    this->listElement->addItem(gpuSubmenu);
    if(!IsHoag()) {
    this->listElement->addItem(new tsl::elm::CategoryHeader("Display"));
        addConfigToggle(HorizonOCConfigValue_OverwriteRefreshRate, nullptr);
        addConfigToggle(HorizonOCConfigValue_EnableUnsafeDisplayFreqs, nullptr);
    }
    #if IS_MINIMAL == 0
        // std::vector<NamedValue> chargerCurrents = {
        //     NamedValue("Disabled", 0),
        //     NamedValue("1024mA", 1024),
        //     NamedValue("1280mA", 1280),
        //     NamedValue("1536mA", 1536),
        //     NamedValue("1792mA", 1792),
        //     NamedValue("2048mA", 2048),
        //     NamedValue("2304mA", 2304),
        //     NamedValue("2560mA", 2560),
        //     NamedValue("2816mA", 2816),
        //     NamedValue("3072mA", 3072),
        // };
        if(this->configList->values[HorizonOCConfigValue_EnableExperimentalSettings]) {
            this->listElement->addItem(new tsl::elm::CategoryHeader("Experimental"));

            addConfigToggle(HorizonOCConfigValue_LiveCpuUv, nullptr);
            std::vector<NamedValue> gpuSchedMethodValues = {
                NamedValue("INI", GpuSchedulingOverrideMethod_Ini),
                NamedValue("NV Service", GpuSchedulingOverrideMethod_NvService),
            };
            addConfigButton(
                HorizonOCConfigValue_GPUSchedulingMethod,
                "GPU Scheduling Override Method",
                ValueRange(0, 0, 1, "", 0),
                "GPU Scheduling Override Method",
                &thresholdsDisabled,
                {},
                gpuSchedMethodValues,
                false
            );

            std::vector<NamedValue> chargerCurrents = {
                NamedValue("Disabled", 0),
                NamedValue("1024mA", 1024),
                NamedValue("1280mA", 1280),
                NamedValue("1536mA", 1536),
                NamedValue("1792mA", 1792),
                NamedValue("2048mA", 2048),
                NamedValue("2304mA", 2304),
                NamedValue("2560mA", 2560),
                NamedValue("2816mA", 2816),
                NamedValue("3072mA", 3072),
                NamedValue("3328mA", 3328),
                NamedValue("3840mA", 3840),
                NamedValue("4096mA", 4096),
                NamedValue("4352mA", 4352),
                NamedValue("4608mA", 4608),
            };

            ValueThresholds chargerThresholds(2048, 2049);

            addConfigButton(
                HorizonOCConfigValue_BatteryChargeCurrent,
                "Charge Current Override",
                ValueRange(0, 0, 1, "", 0),
                "Charge Current Override",
                &chargerThresholds,
                {},
                chargerCurrents,
                false
            );
        }
    #endif
}


class RamSubmenuGui : public MiscGui {
public:
    RamSubmenuGui() { }

protected:
    void listUI() override {
        ValueThresholds thresholdsDisabled(0, 0);
        std::vector<NamedValue> noNamedValues = {};

        this->listElement->addItem(new tsl::elm::CategoryHeader("RAM Settings"));

        addConfigToggle(KipConfigValue_hpMode, "HP Mode");

        ValueThresholds eristaRamThresholds(2208000, 2304000);

        std::vector<NamedValue> marikoMaxEmcClock = {
            NamedValue("1600MHz", 1600000, "JEDEC."),
            NamedValue("1633MHz", 1633000),
            NamedValue("1666MHz", 1666000),
            NamedValue("1700MHz", 1700000),
            NamedValue("1733MHz", 1733000),
            NamedValue("1766MHz", 1766000),
            NamedValue("1800MHz", 1800000),
            NamedValue("1833MHz", 1833000),
            NamedValue("1866MHz", 1866000, "JEDEC."),
            NamedValue("1900MHz", 1900000),
            NamedValue("1933MHz", 1933000),
            NamedValue("1966MHz", 1966000),
            NamedValue("1996MHz", 1996800, "JEDEC."),
            NamedValue("2000MHz", 2000000),
            NamedValue("2033MHz", 2033000),
            NamedValue("2066MHz", 2066000),
            NamedValue("2100MHz", 2100000),
            NamedValue("2133MHz", 2133000, "JEDEC."),
            NamedValue("2166MHz", 2166000),
            NamedValue("2200MHz", 2200000),
            NamedValue("2233MHz", 2233000),
            NamedValue("2266MHz", 2266000),
            NamedValue("2300MHz", 2300000),
            NamedValue("2333MHz", 2333000),
            NamedValue("2366MHz", 2366000),
            NamedValue("2400MHz", 2400000, "JEDEC."),
            NamedValue("2433MHz", 2433000),
            NamedValue("2466MHz", 2466000),
            NamedValue("2500MHz", 2500000),
            NamedValue("2533MHz", 2533000),
            NamedValue("2566MHz", 2566000),
            NamedValue("2600MHz", 2600000),
            NamedValue("2633MHz", 2633000),
            NamedValue("2666MHz", 2666000, "JEDEC."),
            NamedValue("2700MHz", 2700000),
            NamedValue("2733MHz", 2733000),
            NamedValue("2766MHz", 2766000),
            NamedValue("2800MHz", 2800000),
            NamedValue("2833MHz", 2833000),
            NamedValue("2866MHz", 2866000),
            NamedValue("2900MHz", 2900000),
            NamedValue("2933MHz", 2933000, "JEDEC."),
            NamedValue("2966MHz", 2966000),
            NamedValue("3000MHz", 3000000),
            NamedValue("3033MHz", 3033000),
            NamedValue("3066MHz", 3066000),
            NamedValue("3100MHz", 3100000),
            NamedValue("3133MHz", 3133000),
            NamedValue("3166MHz", 3166000),
            NamedValue("3200MHz", 3200000, "JEDEC."),
            NamedValue("3233MHz", 3233000, "High speedo needed!"),
            NamedValue("3266MHz", 3266000, "High speedo needed!"),
            NamedValue("3300MHz", 3300000, "High speedo needed!"),
            NamedValue("3333MHz", 3333000, "High speedo needed!"),
            NamedValue("3366MHz", 3366000, "High speedo needed!"),
            NamedValue("3400MHz", 3400000, "High speedo needed!"),
            NamedValue("3433MHz", 3433000, "High speedo needed!"),
            NamedValue("3466MHz", 3466000, "High speedo needed!"),
            NamedValue("3500MHz", 3500000, "High speedo needed!"),
        };

        std::vector<NamedValue> eristaMaxEmcClock = {
            NamedValue("1600MHz", 1600000, "JEDEC."),
            NamedValue("1633MHz", 1633000),
            NamedValue("1666MHz", 1666000),
            NamedValue("1700MHz", 1700000),
            NamedValue("1733MHz", 1733000),
            NamedValue("1766MHz", 1766000),
            NamedValue("1800MHz", 1800000),
            NamedValue("1833MHz", 1833000),
            NamedValue("1862MHz", 1862400, "JEDEC."),
            NamedValue("1881MHz", 1881600),
            NamedValue("1900MHz", 1900800),
            NamedValue("1920MHz", 1920000),
            NamedValue("1939MHz", 1939200),
            NamedValue("1958MHz", 1958400),
            NamedValue("1977MHz", 1977600),
            NamedValue("1996MHz", 1996800, "JEDEC."),
            NamedValue("2016MHz", 2016000),
            NamedValue("2035MHz", 2035200),
            NamedValue("2054MHz", 2054400),
            NamedValue("2073MHz", 2073600),
            NamedValue("2092MHz", 2092800),
            NamedValue("2112MHz", 2112000),
            NamedValue("2131MHz", 2131200, "JEDEC."),
            NamedValue("2150MHz", 2150400),
            NamedValue("2169MHz", 2169600),
            NamedValue("2188MHz", 2188800),
            NamedValue("2208MHz", 2208000),
            NamedValue("2227MHz", 2227200),
            NamedValue("2246MHz", 2246400),
            NamedValue("2265MHz", 2265600),
            NamedValue("2284MHz", 2284800),
            NamedValue("2304MHz", 2304000),
            NamedValue("2323MHz", 2323200),
            NamedValue("2342MHz", 2342400),
            NamedValue("2361MHz", 2361600),
            NamedValue("2380MHz", 2380800),
            NamedValue("2400MHz", 2400000, "JEDEC."),
            NamedValue("2423MHz", 2423200),
            NamedValue("2442MHz", 2442400),
            NamedValue("2461MHz", 2461600),
            NamedValue("2480MHz", 2480800),
            NamedValue("2500MHz", 2500000),
            NamedValue("2523MHz", 2523200),
            NamedValue("2542MHz", 2542400),
            NamedValue("2561MHz", 2561600),
            NamedValue("2580MHz", 2580800),
            NamedValue("2600MHz", 2600000),
            NamedValue("2623MHz", 2623200),
            NamedValue("2642MHz", 2642400),
            NamedValue("2661MHz", 2661600),
            NamedValue("2680MHz", 2680800),
            NamedValue("2700MHz", 2700000),
        };

        if(IsErista()) {
            addConfigButton(KipConfigValue_eristaEmcMaxClock, "RAM Max Clock", ValueRange(0, 1, 1, "", 1), "RAM Max Clock", &eristaRamThresholds, {}, eristaMaxEmcClock, false);
            addConfigButton(KipConfigValue_eristaEmcMaxClock1, "RAM Max Clock", ValueRange(0, 1, 1, "", 1), "RAM Max Clock", &eristaRamThresholds, {}, eristaMaxEmcClock, false);
            addConfigButton(KipConfigValue_eristaEmcMaxClock2, "RAM Max Clock", ValueRange(0, 1, 1, "", 1), "RAM Max Clock", &eristaRamThresholds, {}, eristaMaxEmcClock, false);
        } else {
            addConfigButton(
                KipConfigValue_marikoEmcMaxClock,
                "RAM Max Clock",
                ValueRange(0, 1, 1, "", 1),
                "RAM Max Clock",
                &thresholdsDisabled,
                {},
                marikoMaxEmcClock,
                false
            );
        }

        std::map<uint32_t, std::string> emc_voltage_label = {
            {1100000, "Default (Mariko)"},
            {1125000, "Default (Erista)"},
            {1175000, "Rating"},
            {1212500, "Safe Max (Mariko)"},
            {1237500, "Safe Max (Erista)"},
            {1250000, "Unsafe Max"},
        };

        ValueThresholds vdd2Thresholds(1212500, 1250000);
        addConfigButton(
            KipConfigValue_commonEmcMemVolt,
            "RAM VDD2 Voltage",
            ValueRange(912500, 1500000, 12500, "mV", 1000, 1),
            "Voltage",
            &vdd2Thresholds,
            emc_voltage_label,
            noNamedValues,
            false
        );

        if(IsMariko()) {
            addConfigButton(
                KipConfigValue_marikoEmcVddqVolt,
                "RAM VDDQ Voltage",
                ValueRange(400000, 1500000, 5000, "mV", 1000),
                "RAM VDDQ Voltage",
                &thresholdsDisabled,
                {},
                {},
                false
            );
        }

        addConfigButton(
            KipConfigValue_emcDvbShift,
            "SoC DVB Shift",
            ValueRange(0, 10, 1, "", 1),
            "SoC DVB Shift",
            &thresholdsDisabled,
            {},
            {},
            false
        );

        tsl::elm::ListItem* timingsSubmenu = new tsl::elm::ListItem("RAM Timing Reductions");
        timingsSubmenu->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<RamTimingsSubmenuGui>();
                return true;
            }
            return false;
        });
        this->listElement->addItem(timingsSubmenu);

        tsl::elm::ListItem* latenciesSubmenu = new tsl::elm::ListItem("RAM Latencies");
        latenciesSubmenu->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<RamLatenciesSubmenuGui>();
                return true;
            }
            return false;
        });
        this->listElement->addItem(latenciesSubmenu);
    }
};

class RamTimingsSubmenuGui : public MiscGui {
public:
    RamTimingsSubmenuGui() { }

protected:
    void listUI() override {
        ValueThresholds thresholdsDisabled(0, 0);

        this->listElement->addItem(new tsl::elm::CategoryHeader("Memory Timings"));

        addConfigButton(KipConfigValue_t1_tRCD, "t1 tRCD", ValueRange(0, 8, 1, "", 1), "tRCD", &thresholdsDisabled, {}, {}, false);
        addConfigButton(KipConfigValue_t2_tRP, "t2 tRP", ValueRange(0, 8, 1, "", 1), "tRP", &thresholdsDisabled, {}, {}, false);
        addConfigButton(KipConfigValue_t3_tRAS, "t3 tRAS", ValueRange(0, 10, 1, "", 1), "tRAS", &thresholdsDisabled, {}, {}, false);
        addConfigButton(KipConfigValue_t4_tRRD, "t4 tRRD", ValueRange(0, 7, 1, "", 1), "tRRD", &thresholdsDisabled, {}, {}, false);
        addConfigButton(KipConfigValue_t5_tRFC, "t5 tRFC", ValueRange(0, 11, 1, "", 1), "tRFC", &thresholdsDisabled, {}, {}, false);
        addConfigButton(KipConfigValue_t6_tRTW, "t6 tRTW", ValueRange(0, 10, 1, "", 1), "tRTW", &thresholdsDisabled, {}, {}, false);
        addConfigButton(KipConfigValue_t7_tWTR, "t7 tWTR", ValueRange(0, 10, 1, "", 1), "tWTR", &thresholdsDisabled, {}, {}, false);
        addConfigButton(KipConfigValue_t8_tREFI, "t8 tREFI", ValueRange(0, 6, 1, "", 1), "tREFI", &thresholdsDisabled, {}, {}, false);

        std::vector<NamedValue> t6_tRTW_fine_tune = {
            NamedValue("-2", 0xFFFFFFFE),
            NamedValue("-1", 0xFFFFFFFF),
            NamedValue(" 0", 0),
            NamedValue("+1", 1),
            NamedValue("+2", 2),
        };

        std::vector<NamedValue> t7_tWTR_fine_tune = {
            NamedValue("-3", 0xFFFFFFFD),
            NamedValue("-2", 0xFFFFFFFE),
            NamedValue("-1", 0xFFFFFFFF),
            NamedValue(" 0", 0),
            NamedValue("+1", 1),
            NamedValue("+2", 2),
            NamedValue("+3", 3),
        };

        this->listElement->addItem(new tsl::elm::CategoryHeader("Advanced"));
        addConfigButton(KipConfigValue_t6_tRTW_fine_tune, "t6 tRTW Fine Tune", ValueRange(0, 4, 1, "", 0), "tRTW Fine Tune", &thresholdsDisabled, {}, t6_tRTW_fine_tune, false);
        addConfigButton(KipConfigValue_t7_tWTR_fine_tune, "t7 tWTR Fine Tune", ValueRange(0, 6, 1, "", 0), "tWTR Fine Tune", &thresholdsDisabled, {}, t7_tWTR_fine_tune, false);
    }
};

class RamLatenciesSubmenuGui : public MiscGui {
public:
    RamLatenciesSubmenuGui() { }

protected:
    void listUI() override {
        ValueThresholds thresholdsDisabled(0, 0);

        this->listElement->addItem(new tsl::elm::CategoryHeader("Memory Latencies"));

        std::vector<NamedValue> rlLabels = {
            NamedValue("1333 RL", 28),
            NamedValue("1600 RL", 32),
            NamedValue("1866 RL", 36),
            NamedValue("2133 RL", 40)
        };

        std::vector<NamedValue> wlLabels = {
            NamedValue("1333 WL", 12),
            NamedValue("1600 WL", 14),
            NamedValue("1866 WL", 16),
            NamedValue("2133 WL", 18)
        };

        addConfigButton(
            KipConfigValue_mem_burst_read_latency,
            "Read Latency",
            ValueRange(0, 6, 1, "", 0),
            "Read Latency",
            &thresholdsDisabled,
            {},
            rlLabels,
            false
        );

        addConfigButton(
            KipConfigValue_mem_burst_write_latency,
            "Write Latency",
            ValueRange(0, 6, 1, "", 0),
            "Write Latency",
            &thresholdsDisabled,
            {},
            wlLabels,
            false
        );
    }
};

class CpuSubmenuGui : public MiscGui {
public:
    CpuSubmenuGui() { }

protected:
    void listUI() override {
        ValueThresholds thresholdsDisabled(0, 0);

        this->listElement->addItem(new tsl::elm::CategoryHeader("CPU Settings"));
        if(IsMariko()) {
            std::vector<NamedValue> ClkOptions = {
                NamedValue("1963 MHz", 1963000),
                NamedValue("2091 MHz", 2091000),
                NamedValue("2193 MHz", 2193000),
                NamedValue("2295 MHz", 2295000),
                NamedValue("2397 MHz", 2397000),
                NamedValue("2499 MHz", 2499000),
                NamedValue("2601 MHz", 2601000),
                NamedValue("2703 MHz", 2703000),
            };
            ValueThresholds mCpuClockThresholds(1963000, 2397000);
            addConfigButton(
                KipConfigValue_marikoCpuBoostClock,
                "CPU Boost Clock",
                ValueRange(0, 0, 1, "", 1),
                "CPU Boost Clock",
                &mCpuClockThresholds,
                {},
                ClkOptions,
                false
            );
        } else {
            std::vector<NamedValue> ClkOptionsE = {
                NamedValue("1785 MHz", 1785000),
                NamedValue("1887 MHz", 1887000),
                NamedValue("1963 MHz", 1963000),
                NamedValue("2091 MHz", 2091000),
                NamedValue("2193 MHz", 2193000),
                NamedValue("2295 MHz", 2295000),
                NamedValue("2397 MHz", 2797000),
                NamedValue("2499 MHz", 2499000),
                NamedValue("2601 MHz", 2601000),
                NamedValue("2703 MHz", 2703000),
            };
            ValueThresholds eCpuClockThresholds(1785000, 2091000);
            addConfigButton(
                KipConfigValue_eristaCpuBoostClock,
                "CPU Boost Clock",
                ValueRange(0, 0, 1, "", 1),
                "CPU Boost Clock",
                &eCpuClockThresholds,
                {},
                ClkOptionsE,
                false
            );
        }
        if(IsErista()) {
            addConfigButton(
                KipConfigValue_eristaCpuUV,
                "CPU UV",
                ValueRange(0, 5, 1, "", 1),
                "CPU UV",
                &thresholdsDisabled,
                {},
                {},
                false
            );

            addConfigToggle(KipConfigValue_eristaCpuUnlock, "CPU Unlock");
            addConfigButton(
                KipConfigValue_eristaCpuVmin,
                "CPU VMIN",
                ValueRange(700, 900, 5, "mV", 1),
                "CPU VMIN",
                &thresholdsDisabled,
                {},
                {},
                false
            );
            addConfigButton(
                KipConfigValue_eristaCpuMaxVolt,
                "CPU Max Voltage",
                ValueRange(1120, 1525, 5, "mV", 1),
                "CPU Max Voltage",
                &thresholdsDisabled,
                {},
                {},
                false
            );
        } else {
            std::vector<NamedValue> marikoTableConf = {
                // NamedValue("Auto", 0),
                NamedValue("Default", 1),
                NamedValue("1581MHz Tbreak", 2),
                NamedValue("1683MHz Tbreak", 3),
                NamedValue("Extreme UV Table", 4)
            };

            addConfigButton(
                KipConfigValue_tableConf,
                "CPU UV Table",
                ValueRange(0, 12, 1, "", 0),
                "CPU UV Table",
                &thresholdsDisabled,
                {},
                marikoTableConf,
                false
            );
            addConfigButton(
                KipConfigValue_marikoCpuUVLow,
                "CPU Low UV",
                ValueRange(0, 8, 1, "", 1),
                "CPU Low UV",
                &thresholdsDisabled,
                {},
                {},
                false
            );
            addConfigButton(
                KipConfigValue_marikoCpuUVHigh,
                "CPU High UV",
                ValueRange(0, 12, 1, "", 1),
                "CPU High UV",
                &thresholdsDisabled,
                {},
                {},
                false
            );

            std::vector<NamedValue> maxClkOptions = {
                NamedValue("1785 MHz", 1785000),
                NamedValue("1887 MHz", 1887000),
                NamedValue("1963 MHz", 1963000),
                NamedValue("2091 MHz", 2091000),
                NamedValue("2193 MHz", 2193000),
                NamedValue("2295 MHz", 2295000),
                NamedValue("2397 MHz", 2397000),
                NamedValue("2499 MHz", 2499000),
                NamedValue("2601 MHz", 2601000),
                NamedValue("2703 MHz", 2703000),
                NamedValue("2805 MHz", 2805000),
                NamedValue("2907 MHz", 2907000),
                NamedValue("3009 MHz", 3009000),
            };
            ValueThresholds mCpuMaxClockThresholds(1963000, 2397000);
            addConfigButton(
                KipConfigValue_marikoCpuMaxClock,
                "CPU Max Clock",
                ValueRange(0, 0, 1, "", 1),
                "CPU Max Clock",
                &mCpuMaxClockThresholds,
                {},
                maxClkOptions,
                false
            );

            addConfigButton(
                KipConfigValue_marikoCpuLowVmin,
                "CPU Low VMIN",
                ValueRange(550, 750, 5, "mV", 1),
                "CPU VMIN",
                &thresholdsDisabled,
                {},
                {},
                false
            );

            addConfigButton(
                KipConfigValue_marikoCpuHighVmin,
                "CPU High VMIN",
                ValueRange(650, 900, 5, "mV", 1),
                "CPU VMIN",
                &thresholdsDisabled,
                {},
                {},
                false
            );

            ValueThresholds mCpuVoltThresholds(1160, 1180);
            addConfigButton(
                KipConfigValue_marikoCpuMaxVolt,
                "CPU Max Voltage",
                ValueRange(1000, 1525, 5, "mV", 1),
                "CPU Max Voltage",
                &mCpuVoltThresholds,
                {},
                {},
                false
            );
        }
    }
};

class GpuSubmenuGui : public MiscGui {
public:
    GpuSubmenuGui() { }

protected:
    void listUI() override {
        ValueThresholds thresholdsDisabled(0, 0);
        std::vector<NamedValue> noNamedValues = {};

        this->listElement->addItem(new tsl::elm::CategoryHeader("GPU Settings"));

        std::vector<NamedValue> gpuUvConf = {
            NamedValue("No Undervolt", 0),
            NamedValue("SLT Table", 1),
            NamedValue("HiOPT Table", 2),
        };

        std::vector<NamedValue> mGpuVoltsVmin = {
            NamedValue("Auto", 0),
            NamedValue("480mV", 480), NamedValue("485mV", 485), NamedValue("490mV", 490),
            NamedValue("495mV", 495), NamedValue("500mV", 500), NamedValue("505mV", 505),
            NamedValue("510mV", 510), NamedValue("515mV", 515), NamedValue("520mV", 520),
            NamedValue("525mV", 525), NamedValue("530mV", 530), NamedValue("535mV", 535),
            NamedValue("540mV", 540), NamedValue("545mV", 545), NamedValue("550mV", 550),
            NamedValue("555mV", 555), NamedValue("560mV", 560), NamedValue("565mV", 565),
            NamedValue("570mV", 570), NamedValue("575mV", 575), NamedValue("580mV", 580),
            NamedValue("585mV", 585), NamedValue("590mV", 590), NamedValue("595mV", 595),
            NamedValue("600mV", 600), NamedValue("605mV", 605), NamedValue("610mV", 610),
            NamedValue("615mV", 615), NamedValue("620mV", 620), NamedValue("625mV", 625),
            NamedValue("630mV", 630), NamedValue("635mV", 635), NamedValue("640mV", 640),
            NamedValue("645mV", 645), NamedValue("650mV", 650), NamedValue("655mV", 655),
            NamedValue("660mV", 660), NamedValue("665mV", 665), NamedValue("670mV", 670),
            NamedValue("675mV", 675), NamedValue("680mV", 680), NamedValue("685mV", 685),
            NamedValue("690mV", 690), NamedValue("695mV", 695), NamedValue("700mV", 700),
            NamedValue("705mV", 705), NamedValue("710mV", 710), NamedValue("715mV", 715),
            NamedValue("720mV", 720), NamedValue("725mV", 725), NamedValue("730mV", 730),
            NamedValue("735mV", 735), NamedValue("740mV", 740), NamedValue("745mV", 745),
            NamedValue("750mV", 750), NamedValue("755mV", 755), NamedValue("760mV", 760),
            NamedValue("765mV", 765), NamedValue("770mV", 770), NamedValue("775mV", 775),
            NamedValue("780mV", 780), NamedValue("785mV", 785), NamedValue("790mV", 790),
            NamedValue("795mV", 795), NamedValue("800mV", 800)
        };

        if(IsErista()) {
            addConfigButton(
                KipConfigValue_eristaGpuUV,
                "GPU Undervolt Table",
                ValueRange(0, 1, 1, "", 1),
                "GPU Undervolt Table",
                &thresholdsDisabled,
                {},
                gpuUvConf,
                false
            );
            addConfigButton(
                KipConfigValue_eristaGpuVmin,
                "GPU Minimum Voltage",
                ValueRange(700, 875, 5, "mV", 1),
                "GPU Minimum Voltage",
                &thresholdsDisabled,
                {},
                {},
                false
            );
        } else {
            addConfigButton(
                KipConfigValue_marikoGpuUV,
                "GPU Undervolt Table",
                ValueRange(0, 1, 1, "", 1),
                "GPU Undervolt Table",
                &thresholdsDisabled,
                {},
                gpuUvConf,
                false
            );

            // tsl::elm::ListItem* vminCalcBtn = new tsl::elm::ListItem("Calculate GPU Vmin");
            // vminCalcBtn->setClickListener([this](u64 keys) {
            //     if (keys & HidNpadButton_A) {
            //         Result rc = hocClkIpcCalculateGpuVmin();
            //         if (R_FAILED(rc)) {
            //             FatalGui::openWithResultCode("hocClkIpcCalculateGpuVmin", rc);
            //             return false;
            //         }
            //         return true;
            //     }
            //     return false;
            // });

            addConfigButton(KipConfigValue_marikoGpuVmin, "GPU VMIN", ValueRange(0, 0, 0, "0", 1), "GPU VMIN", &thresholdsDisabled, {}, mGpuVoltsVmin, false);
            ValueThresholds MgpuVmaxThresholds(800, 850);
            addConfigButton(
                KipConfigValue_marikoGpuVmax,
                "GPU Maximum Voltage",
                ValueRange(750, 1525, 5, "mV", 1),
                "GPU Maximum Voltage",
                &MgpuVmaxThresholds,
                {},
                {},
                false
            );
        }

        addConfigButton(
            KipConfigValue_commonGpuVoltOffset,
            "GPU Voltage Offset",
            ValueRange(0, 50, 5, "mV", 1),
            "GPU Voltage Offset",
            &thresholdsDisabled,
            {},
            {},
            false
        );

        tsl::elm::ListItem* customTableSubmenu = new tsl::elm::ListItem("GPU Voltage Table");
        customTableSubmenu->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<GpuCustomTableSubmenuGui>();
                return true;
            }
            return false;
        });
        this->listElement->addItem(customTableSubmenu);
    }
};

class GpuCustomTableSubmenuGui : public MiscGui {
public:
    GpuCustomTableSubmenuGui() { }

protected:
    void listUI() override {

        Result rc = sysclkIpcGetConfigValues(this->configList); // populate config list early otherwise wont work
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("sysclkIpcGetConfigValues", rc);
            return;
        }

        this->listElement->addItem(new tsl::elm::CategoryHeader("GPU Custom Table (mV)"));

        ValueThresholds MgpuVmaxThresholds(800, 850);
        ValueThresholds EgpuVmaxThresholds(950, 975);

        std::vector<NamedValue> mGpuVolts = {
            NamedValue("Disabled", 2000),
            NamedValue("Auto", 0),
            NamedValue("480mV", 480), NamedValue("485mV", 485), NamedValue("490mV", 490),
            NamedValue("495mV", 495), NamedValue("500mV", 500), NamedValue("505mV", 505),
            NamedValue("510mV", 510), NamedValue("515mV", 515), NamedValue("520mV", 520),
            NamedValue("525mV", 525), NamedValue("530mV", 530), NamedValue("535mV", 535),
            NamedValue("540mV", 540), NamedValue("545mV", 545), NamedValue("550mV", 550),
            NamedValue("555mV", 555), NamedValue("560mV", 560), NamedValue("565mV", 565),
            NamedValue("570mV", 570), NamedValue("575mV", 575), NamedValue("580mV", 580),
            NamedValue("585mV", 585), NamedValue("590mV", 590), NamedValue("595mV", 595),
            NamedValue("600mV", 600), NamedValue("605mV", 605), NamedValue("610mV", 610),
            NamedValue("615mV", 615), NamedValue("620mV", 620), NamedValue("625mV", 625),
            NamedValue("630mV", 630), NamedValue("635mV", 635), NamedValue("640mV", 640),
            NamedValue("645mV", 645), NamedValue("650mV", 650), NamedValue("655mV", 655),
            NamedValue("660mV", 660), NamedValue("665mV", 665), NamedValue("670mV", 670),
            NamedValue("675mV", 675), NamedValue("680mV", 680), NamedValue("685mV", 685),
            NamedValue("690mV", 690), NamedValue("695mV", 695), NamedValue("700mV", 700),
            NamedValue("705mV", 705), NamedValue("710mV", 710), NamedValue("715mV", 715),
            NamedValue("720mV", 720), NamedValue("725mV", 725), NamedValue("730mV", 730),
            NamedValue("735mV", 735), NamedValue("740mV", 740), NamedValue("745mV", 745),
            NamedValue("750mV", 750), NamedValue("755mV", 755), NamedValue("760mV", 760),
            NamedValue("765mV", 765), NamedValue("770mV", 770), NamedValue("775mV", 775),
            NamedValue("780mV", 780), NamedValue("785mV", 785), NamedValue("790mV", 790),
            NamedValue("795mV", 795), NamedValue("800mV", 800), NamedValue("805mV", 805),
            NamedValue("810mV", 810), NamedValue("815mV", 815), NamedValue("820mV", 820),
            NamedValue("825mV", 825), NamedValue("830mV", 830), NamedValue("835mV", 835),
            NamedValue("840mV", 840), NamedValue("845mV", 845), NamedValue("850mV", 850),
            NamedValue("855mV", 855), NamedValue("860mV", 860), NamedValue("865mV", 865),
            NamedValue("870mV", 870), NamedValue("875mV", 875), NamedValue("880mV", 880),
            NamedValue("885mV", 885), NamedValue("890mV", 890), NamedValue("895mV", 895),
            NamedValue("900mV", 900), NamedValue("905mV", 905), NamedValue("910mV", 910),
            NamedValue("915mV", 915), NamedValue("920mV", 920), NamedValue("925mV", 925),
            NamedValue("930mV", 930), NamedValue("935mV", 935), NamedValue("940mV", 940),
            NamedValue("945mV", 945), NamedValue("950mV", 950), NamedValue("955mV", 955),
            NamedValue("960mV", 960), NamedValue("965mV", 965), NamedValue("970mV", 970),
            NamedValue("975mV", 975), NamedValue("980mV", 980), NamedValue("985mV", 985),
            NamedValue("990mV", 990), NamedValue("995mV", 995), NamedValue("1000mV", 1000),
            NamedValue("1005mV", 1005), NamedValue("1010mV", 1010), NamedValue("1015mV", 1015),
            NamedValue("1020mV", 1020), NamedValue("1025mV", 1025), NamedValue("1030mV", 1030),
            NamedValue("1035mV", 1035), NamedValue("1040mV", 1040), NamedValue("1045mV", 1045),
            NamedValue("1050mV", 1050), NamedValue("1055mV", 1055), NamedValue("1060mV", 1060),
            NamedValue("1065mV", 1065), NamedValue("1070mV", 1070), NamedValue("1075mV", 1075),
            NamedValue("1080mV", 1080), NamedValue("1085mV", 1085), NamedValue("1090mV", 1090),
            NamedValue("1095mV", 1095), NamedValue("1100mV", 1100), NamedValue("1105mV", 1105),
            NamedValue("1110mV", 1110), NamedValue("1115mV", 1115), NamedValue("1120mV", 1120),
            NamedValue("1125mV", 1125), NamedValue("1130mV", 1130), NamedValue("1135mV", 1135),
            NamedValue("1140mV", 1140), NamedValue("1145mV", 1145), NamedValue("1150mV", 1150),
            NamedValue("1155mV", 1155), NamedValue("1160mV", 1160), NamedValue("1165mV", 1165),
            NamedValue("1170mV", 1170), NamedValue("1175mV", 1175), NamedValue("1180mV", 1180),
            NamedValue("1185mV", 1185), NamedValue("1190mV", 1190), NamedValue("1195mV", 1195),
            NamedValue("1200mV", 1200), NamedValue("1205mV", 1205), NamedValue("1210mV", 1210),
            NamedValue("1215mV", 1215), NamedValue("1220mV", 1220), NamedValue("1225mV", 1225),
            NamedValue("1230mV", 1230), NamedValue("1235mV", 1235), NamedValue("1240mV", 1240),
            NamedValue("1245mV", 1245), NamedValue("1250mV", 1250), NamedValue("1255mV", 1255),
            NamedValue("1260mV", 1260), NamedValue("1265mV", 1265), NamedValue("1270mV", 1270),
            NamedValue("1275mV", 1275), NamedValue("1280mV", 1280), NamedValue("1285mV", 1285),
            NamedValue("1290mV", 1290), NamedValue("1295mV", 1295), NamedValue("1300mV", 1300),
            NamedValue("1305mV", 1305), NamedValue("1310mV", 1310), NamedValue("1315mV", 1315),
            NamedValue("1320mV", 1320), NamedValue("1325mV", 1325), NamedValue("1330mV", 1330),
            NamedValue("1335mV", 1335), NamedValue("1340mV", 1340), NamedValue("1345mV", 1345),
            NamedValue("1350mV", 1350), NamedValue("1355mV", 1355), NamedValue("1360mV", 1360),
            NamedValue("1365mV", 1365), NamedValue("1370mV", 1370), NamedValue("1375mV", 1375),
            NamedValue("1380mV", 1380), NamedValue("1385mV", 1385), NamedValue("1390mV", 1390),
            NamedValue("1395mV", 1395), NamedValue("1400mV", 1400), NamedValue("1405mV", 1405),
            NamedValue("1410mV", 1410), NamedValue("1415mV", 1415), NamedValue("1420mV", 1420),
            NamedValue("1425mV", 1425), NamedValue("1430mV", 1430), NamedValue("1435mV", 1435),
            NamedValue("1440mV", 1440), NamedValue("1445mV", 1445), NamedValue("1450mV", 1450),
            NamedValue("1455mV", 1455), NamedValue("1460mV", 1460), NamedValue("1465mV", 1465),
            NamedValue("1470mV", 1470), NamedValue("1475mV", 1475), NamedValue("1480mV", 1480),
            NamedValue("1485mV", 1485), NamedValue("1490mV", 1490), NamedValue("1495mV", 1495),
            NamedValue("1500mV", 1500), NamedValue("1505mV", 1505), NamedValue("1510mV", 1510),
            NamedValue("1515mV", 1515), NamedValue("1520mV", 1520), NamedValue("1525mV", 1525),
        };

        std::vector<NamedValue> eGpuVolts = {
            NamedValue("Disabled", 2000),
            NamedValue("Auto", 0),
            NamedValue("480mV", 480), NamedValue("485mV", 485), NamedValue("490mV", 490),
            NamedValue("495mV", 495), NamedValue("500mV", 500), NamedValue("505mV", 505),
            NamedValue("510mV", 510), NamedValue("515mV", 515), NamedValue("520mV", 520),
            NamedValue("525mV", 525), NamedValue("530mV", 530), NamedValue("535mV", 535),
            NamedValue("540mV", 540), NamedValue("545mV", 545), NamedValue("550mV", 550),
            NamedValue("555mV", 555), NamedValue("560mV", 560), NamedValue("565mV", 565),
            NamedValue("570mV", 570), NamedValue("575mV", 575), NamedValue("580mV", 580),
            NamedValue("585mV", 585), NamedValue("590mV", 590), NamedValue("595mV", 595),
            NamedValue("600mV", 600), NamedValue("605mV", 605), NamedValue("610mV", 610),
            NamedValue("615mV", 615), NamedValue("620mV", 620), NamedValue("625mV", 625),
            NamedValue("630mV", 630), NamedValue("635mV", 635), NamedValue("640mV", 640),
            NamedValue("645mV", 645), NamedValue("650mV", 650), NamedValue("655mV", 655),
            NamedValue("660mV", 660), NamedValue("665mV", 665), NamedValue("670mV", 670),
            NamedValue("675mV", 675), NamedValue("680mV", 680), NamedValue("685mV", 685),
            NamedValue("690mV", 690), NamedValue("695mV", 695), NamedValue("700mV", 700),
            NamedValue("705mV", 705), NamedValue("710mV", 710), NamedValue("715mV", 715),
            NamedValue("720mV", 720), NamedValue("725mV", 725), NamedValue("730mV", 730),
            NamedValue("735mV", 735), NamedValue("740mV", 740), NamedValue("745mV", 745),
            NamedValue("750mV", 750), NamedValue("755mV", 755), NamedValue("760mV", 760),
            NamedValue("765mV", 765), NamedValue("770mV", 770), NamedValue("775mV", 775),
            NamedValue("780mV", 780), NamedValue("785mV", 785), NamedValue("790mV", 790),
            NamedValue("795mV", 795), NamedValue("800mV", 800), NamedValue("805mV", 805),
            NamedValue("810mV", 810), NamedValue("815mV", 815), NamedValue("820mV", 820),
            NamedValue("825mV", 825), NamedValue("830mV", 830), NamedValue("835mV", 835),
            NamedValue("840mV", 840), NamedValue("845mV", 845), NamedValue("850mV", 850),
            NamedValue("855mV", 855), NamedValue("860mV", 860), NamedValue("865mV", 865),
            NamedValue("870mV", 870), NamedValue("875mV", 875), NamedValue("880mV", 880),
            NamedValue("885mV", 885), NamedValue("890mV", 890), NamedValue("895mV", 895),
            NamedValue("900mV", 900), NamedValue("905mV", 905), NamedValue("910mV", 910),
            NamedValue("915mV", 915), NamedValue("920mV", 920), NamedValue("925mV", 925),
            NamedValue("930mV", 930), NamedValue("935mV", 935), NamedValue("940mV", 940),
            NamedValue("945mV", 945), NamedValue("950mV", 950), NamedValue("955mV", 955),
            NamedValue("960mV", 960), NamedValue("965mV", 965), NamedValue("970mV", 970),
            NamedValue("975mV", 975), NamedValue("980mV", 980), NamedValue("985mV", 985),
            NamedValue("990mV", 990), NamedValue("995mV", 995), NamedValue("1000mV", 1000),
            NamedValue("1005mV", 1005), NamedValue("1010mV", 1010), NamedValue("1015mV", 1015),
            NamedValue("1020mV", 1020), NamedValue("1025mV", 1025), NamedValue("1030mV", 1030),
            NamedValue("1035mV", 1035), NamedValue("1040mV", 1040), NamedValue("1045mV", 1045),
            NamedValue("1050mV", 1050), NamedValue("1055mV", 1055), NamedValue("1060mV", 1060),
            NamedValue("1065mV", 1065), NamedValue("1070mV", 1070), NamedValue("1075mV", 1075),
            NamedValue("1080mV", 1080), NamedValue("1085mV", 1085), NamedValue("1090mV", 1090),
            NamedValue("1095mV", 1095), NamedValue("1100mV", 1100), NamedValue("1105mV", 1105),
            NamedValue("1110mV", 1110), NamedValue("1115mV", 1115), NamedValue("1120mV", 1120),
            NamedValue("1125mV", 1125), NamedValue("1130mV", 1130), NamedValue("1135mV", 1135),
            NamedValue("1140mV", 1140), NamedValue("1145mV", 1145), NamedValue("1150mV", 1150),
            NamedValue("1155mV", 1155), NamedValue("1160mV", 1160), NamedValue("1165mV", 1165),
            NamedValue("1170mV", 1170), NamedValue("1175mV", 1175), NamedValue("1180mV", 1180),
            NamedValue("1185mV", 1185), NamedValue("1190mV", 1190), NamedValue("1195mV", 1195),
            NamedValue("1200mV", 1200), NamedValue("1205mV", 1205), NamedValue("1210mV", 1210),
            NamedValue("1215mV", 1215), NamedValue("1220mV", 1220), NamedValue("1225mV", 1225),
            NamedValue("1230mV", 1230), NamedValue("1235mV", 1235), NamedValue("1240mV", 1240),
            NamedValue("1245mV", 1245), NamedValue("1250mV", 1250), NamedValue("1255mV", 1255),
            NamedValue("1260mV", 1260), NamedValue("1265mV", 1265), NamedValue("1270mV", 1270),
            NamedValue("1275mV", 1275), NamedValue("1280mV", 1280), NamedValue("1285mV", 1285),
            NamedValue("1290mV", 1290), NamedValue("1295mV", 1295), NamedValue("1300mV", 1300),
            NamedValue("1305mV", 1305), NamedValue("1310mV", 1310), NamedValue("1315mV", 1315),
            NamedValue("1320mV", 1320), NamedValue("1325mV", 1325), NamedValue("1330mV", 1330),
            NamedValue("1335mV", 1335), NamedValue("1340mV", 1340), NamedValue("1345mV", 1345),
            NamedValue("1350mV", 1350), NamedValue("1355mV", 1355), NamedValue("1360mV", 1360),
            NamedValue("1365mV", 1365), NamedValue("1370mV", 1370), NamedValue("1375mV", 1375),
            NamedValue("1380mV", 1380), NamedValue("1385mV", 1385), NamedValue("1390mV", 1390),
            NamedValue("1395mV", 1395), NamedValue("1400mV", 1400), NamedValue("1405mV", 1405),
            NamedValue("1410mV", 1410), NamedValue("1415mV", 1415), NamedValue("1420mV", 1420),
            NamedValue("1425mV", 1425), NamedValue("1430mV", 1430), NamedValue("1435mV", 1435),
            NamedValue("1440mV", 1440), NamedValue("1445mV", 1445), NamedValue("1450mV", 1450),
            NamedValue("1455mV", 1455), NamedValue("1460mV", 1460), NamedValue("1465mV", 1465),
            NamedValue("1470mV", 1470), NamedValue("1475mV", 1475), NamedValue("1480mV", 1480),
            NamedValue("1485mV", 1485), NamedValue("1490mV", 1490), NamedValue("1495mV", 1495),
            NamedValue("1500mV", 1500), NamedValue("1505mV", 1505), NamedValue("1510mV", 1510),
            NamedValue("1515mV", 1515), NamedValue("1520mV", 1520), NamedValue("1525mV", 1525),
        };

        std::vector<NamedValue> mGpuVolts_noAuto = {
            NamedValue("Disabled", 2000),
            NamedValue("480mV", 480), NamedValue("485mV", 485), NamedValue("490mV", 490),
            NamedValue("495mV", 495), NamedValue("500mV", 500), NamedValue("505mV", 505),
            NamedValue("510mV", 510), NamedValue("515mV", 515), NamedValue("520mV", 520),
            NamedValue("525mV", 525), NamedValue("530mV", 530), NamedValue("535mV", 535),
            NamedValue("540mV", 540), NamedValue("545mV", 545), NamedValue("550mV", 550),
            NamedValue("555mV", 555), NamedValue("560mV", 560), NamedValue("565mV", 565),
            NamedValue("570mV", 570), NamedValue("575mV", 575), NamedValue("580mV", 580),
            NamedValue("585mV", 585), NamedValue("590mV", 590), NamedValue("595mV", 595),
            NamedValue("600mV", 600), NamedValue("605mV", 605), NamedValue("610mV", 610),
            NamedValue("615mV", 615), NamedValue("620mV", 620), NamedValue("625mV", 625),
            NamedValue("630mV", 630), NamedValue("635mV", 635), NamedValue("640mV", 640),
            NamedValue("645mV", 645), NamedValue("650mV", 650), NamedValue("655mV", 655),
            NamedValue("660mV", 660), NamedValue("665mV", 665), NamedValue("670mV", 670),
            NamedValue("675mV", 675), NamedValue("680mV", 680), NamedValue("685mV", 685),
            NamedValue("690mV", 690), NamedValue("695mV", 695), NamedValue("700mV", 700),
            NamedValue("705mV", 705), NamedValue("710mV", 710), NamedValue("715mV", 715),
            NamedValue("720mV", 720), NamedValue("725mV", 725), NamedValue("730mV", 730),
            NamedValue("735mV", 735), NamedValue("740mV", 740), NamedValue("745mV", 745),
            NamedValue("750mV", 750), NamedValue("755mV", 755), NamedValue("760mV", 760),
            NamedValue("765mV", 765), NamedValue("770mV", 770), NamedValue("775mV", 775),
            NamedValue("780mV", 780), NamedValue("785mV", 785), NamedValue("790mV", 790),
            NamedValue("795mV", 795), NamedValue("800mV", 800), NamedValue("805mV", 805),
            NamedValue("810mV", 810), NamedValue("815mV", 815), NamedValue("820mV", 820),
            NamedValue("825mV", 825), NamedValue("830mV", 830), NamedValue("835mV", 835),
            NamedValue("840mV", 840), NamedValue("845mV", 845), NamedValue("850mV", 850),
            NamedValue("855mV", 855), NamedValue("860mV", 860), NamedValue("865mV", 865),
            NamedValue("870mV", 870), NamedValue("875mV", 875), NamedValue("880mV", 880),
            NamedValue("885mV", 885), NamedValue("890mV", 890), NamedValue("895mV", 895),
            NamedValue("900mV", 900), NamedValue("905mV", 905), NamedValue("910mV", 910),
            NamedValue("915mV", 915), NamedValue("920mV", 920), NamedValue("925mV", 925),
            NamedValue("930mV", 930), NamedValue("935mV", 935), NamedValue("940mV", 940),
            NamedValue("945mV", 945), NamedValue("950mV", 950), NamedValue("955mV", 955),
            NamedValue("960mV", 960), NamedValue("965mV", 965), NamedValue("970mV", 970),
            NamedValue("975mV", 975), NamedValue("980mV", 980), NamedValue("985mV", 985),
            NamedValue("990mV", 990), NamedValue("995mV", 995), NamedValue("1000mV", 1000),
            NamedValue("1005mV", 1005), NamedValue("1010mV", 1010), NamedValue("1015mV", 1015),
            NamedValue("1020mV", 1020), NamedValue("1025mV", 1025), NamedValue("1030mV", 1030),
            NamedValue("1035mV", 1035), NamedValue("1040mV", 1040), NamedValue("1045mV", 1045),
            NamedValue("1050mV", 1050), NamedValue("1055mV", 1055), NamedValue("1060mV", 1060),
            NamedValue("1065mV", 1065), NamedValue("1070mV", 1070), NamedValue("1075mV", 1075),
            NamedValue("1080mV", 1080), NamedValue("1085mV", 1085), NamedValue("1090mV", 1090),
            NamedValue("1095mV", 1095), NamedValue("1100mV", 1100), NamedValue("1105mV", 1105),
            NamedValue("1110mV", 1110), NamedValue("1115mV", 1115), NamedValue("1120mV", 1120),
            NamedValue("1125mV", 1125), NamedValue("1130mV", 1130), NamedValue("1135mV", 1135),
            NamedValue("1140mV", 1140), NamedValue("1145mV", 1145), NamedValue("1150mV", 1150),
            NamedValue("1155mV", 1155), NamedValue("1160mV", 1160), NamedValue("1165mV", 1165),
            NamedValue("1170mV", 1170), NamedValue("1175mV", 1175), NamedValue("1180mV", 1180),
            NamedValue("1185mV", 1185), NamedValue("1190mV", 1190), NamedValue("1195mV", 1195),
            NamedValue("1200mV", 1200), NamedValue("1205mV", 1205), NamedValue("1210mV", 1210),
            NamedValue("1215mV", 1215), NamedValue("1220mV", 1220), NamedValue("1225mV", 1225),
            NamedValue("1230mV", 1230), NamedValue("1235mV", 1235), NamedValue("1240mV", 1240),
            NamedValue("1245mV", 1245), NamedValue("1250mV", 1250), NamedValue("1255mV", 1255),
            NamedValue("1260mV", 1260), NamedValue("1265mV", 1265), NamedValue("1270mV", 1270),
            NamedValue("1275mV", 1275), NamedValue("1280mV", 1280), NamedValue("1285mV", 1285),
            NamedValue("1290mV", 1290), NamedValue("1295mV", 1295), NamedValue("1300mV", 1300),
            NamedValue("1305mV", 1305), NamedValue("1310mV", 1310), NamedValue("1315mV", 1315),
            NamedValue("1320mV", 1320), NamedValue("1325mV", 1325), NamedValue("1330mV", 1330),
            NamedValue("1335mV", 1335), NamedValue("1340mV", 1340), NamedValue("1345mV", 1345),
            NamedValue("1350mV", 1350), NamedValue("1355mV", 1355), NamedValue("1360mV", 1360),
            NamedValue("1365mV", 1365), NamedValue("1370mV", 1370), NamedValue("1375mV", 1375),
            NamedValue("1380mV", 1380), NamedValue("1385mV", 1385), NamedValue("1390mV", 1390),
            NamedValue("1395mV", 1395), NamedValue("1400mV", 1400), NamedValue("1405mV", 1405),
            NamedValue("1410mV", 1410), NamedValue("1415mV", 1415), NamedValue("1420mV", 1420),
            NamedValue("1425mV", 1425), NamedValue("1430mV", 1430), NamedValue("1435mV", 1435),
            NamedValue("1440mV", 1440), NamedValue("1445mV", 1445), NamedValue("1450mV", 1450),
            NamedValue("1455mV", 1455), NamedValue("1460mV", 1460), NamedValue("1465mV", 1465),
            NamedValue("1470mV", 1470), NamedValue("1475mV", 1475), NamedValue("1480mV", 1480),
            NamedValue("1485mV", 1485), NamedValue("1490mV", 1490), NamedValue("1495mV", 1495),
            NamedValue("1500mV", 1500), NamedValue("1505mV", 1505), NamedValue("1510mV", 1510),
            NamedValue("1515mV", 1515), NamedValue("1520mV", 1520), NamedValue("1525mV", 1525),
        };

        std::vector<NamedValue> eGpuVolts_noAuto = {
            NamedValue("Disabled", 2000),
            NamedValue("480mV", 480), NamedValue("485mV", 485), NamedValue("490mV", 490),
            NamedValue("495mV", 495), NamedValue("500mV", 500), NamedValue("505mV", 505),
            NamedValue("510mV", 510), NamedValue("515mV", 515), NamedValue("520mV", 520),
            NamedValue("525mV", 525), NamedValue("530mV", 530), NamedValue("535mV", 535),
            NamedValue("540mV", 540), NamedValue("545mV", 545), NamedValue("550mV", 550),
            NamedValue("555mV", 555), NamedValue("560mV", 560), NamedValue("565mV", 565),
            NamedValue("570mV", 570), NamedValue("575mV", 575), NamedValue("580mV", 580),
            NamedValue("585mV", 585), NamedValue("590mV", 590), NamedValue("595mV", 595),
            NamedValue("600mV", 600), NamedValue("605mV", 605), NamedValue("610mV", 610),
            NamedValue("615mV", 615), NamedValue("620mV", 620), NamedValue("625mV", 625),
            NamedValue("630mV", 630), NamedValue("635mV", 635), NamedValue("640mV", 640),
            NamedValue("645mV", 645), NamedValue("650mV", 650), NamedValue("655mV", 655),
            NamedValue("660mV", 660), NamedValue("665mV", 665), NamedValue("670mV", 670),
            NamedValue("675mV", 675), NamedValue("680mV", 680), NamedValue("685mV", 685),
            NamedValue("690mV", 690), NamedValue("695mV", 695), NamedValue("700mV", 700),
            NamedValue("705mV", 705), NamedValue("710mV", 710), NamedValue("715mV", 715),
            NamedValue("720mV", 720), NamedValue("725mV", 725), NamedValue("730mV", 730),
            NamedValue("735mV", 735), NamedValue("740mV", 740), NamedValue("745mV", 745),
            NamedValue("750mV", 750), NamedValue("755mV", 755), NamedValue("760mV", 760),
            NamedValue("765mV", 765), NamedValue("770mV", 770), NamedValue("775mV", 775),
            NamedValue("780mV", 780), NamedValue("785mV", 785), NamedValue("790mV", 790),
            NamedValue("795mV", 795), NamedValue("800mV", 800), NamedValue("805mV", 805),
            NamedValue("810mV", 810), NamedValue("815mV", 815), NamedValue("820mV", 820),
            NamedValue("825mV", 825), NamedValue("830mV", 830), NamedValue("835mV", 835),
            NamedValue("840mV", 840), NamedValue("845mV", 845), NamedValue("850mV", 850),
            NamedValue("855mV", 855), NamedValue("860mV", 860), NamedValue("865mV", 865),
            NamedValue("870mV", 870), NamedValue("875mV", 875), NamedValue("880mV", 880),
            NamedValue("885mV", 885), NamedValue("890mV", 890), NamedValue("895mV", 895),
            NamedValue("900mV", 900), NamedValue("905mV", 905), NamedValue("910mV", 910),
            NamedValue("915mV", 915), NamedValue("920mV", 920), NamedValue("925mV", 925),
            NamedValue("930mV", 930), NamedValue("935mV", 935), NamedValue("940mV", 940),
            NamedValue("945mV", 945), NamedValue("950mV", 950), NamedValue("955mV", 955),
            NamedValue("960mV", 960), NamedValue("965mV", 965), NamedValue("970mV", 970),
            NamedValue("975mV", 975), NamedValue("980mV", 980), NamedValue("985mV", 985),
            NamedValue("990mV", 990), NamedValue("995mV", 995), NamedValue("1000mV", 1000),
            NamedValue("1005mV", 1005), NamedValue("1010mV", 1010), NamedValue("1015mV", 1015),
            NamedValue("1020mV", 1020), NamedValue("1025mV", 1025), NamedValue("1030mV", 1030),
            NamedValue("1035mV", 1035), NamedValue("1040mV", 1040), NamedValue("1045mV", 1045),
            NamedValue("1050mV", 1050), NamedValue("1055mV", 1055), NamedValue("1060mV", 1060),
            NamedValue("1065mV", 1065), NamedValue("1070mV", 1070), NamedValue("1075mV", 1075),
            NamedValue("1080mV", 1080), NamedValue("1085mV", 1085), NamedValue("1090mV", 1090),
            NamedValue("1095mV", 1095), NamedValue("1100mV", 1100), NamedValue("1105mV", 1105),
            NamedValue("1110mV", 1110), NamedValue("1115mV", 1115), NamedValue("1120mV", 1120),
            NamedValue("1125mV", 1125), NamedValue("1130mV", 1130), NamedValue("1135mV", 1135),
            NamedValue("1140mV", 1140), NamedValue("1145mV", 1145), NamedValue("1150mV", 1150),
            NamedValue("1155mV", 1155), NamedValue("1160mV", 1160), NamedValue("1165mV", 1165),
            NamedValue("1170mV", 1170), NamedValue("1175mV", 1175), NamedValue("1180mV", 1180),
            NamedValue("1185mV", 1185), NamedValue("1190mV", 1190), NamedValue("1195mV", 1195),
            NamedValue("1200mV", 1200), NamedValue("1205mV", 1205), NamedValue("1210mV", 1210),
            NamedValue("1215mV", 1215), NamedValue("1220mV", 1220), NamedValue("1225mV", 1225),
            NamedValue("1230mV", 1230), NamedValue("1235mV", 1235), NamedValue("1240mV", 1240),
            NamedValue("1245mV", 1245), NamedValue("1250mV", 1250), NamedValue("1255mV", 1255),
            NamedValue("1260mV", 1260), NamedValue("1265mV", 1265), NamedValue("1270mV", 1270),
            NamedValue("1275mV", 1275), NamedValue("1280mV", 1280), NamedValue("1285mV", 1285),
            NamedValue("1290mV", 1290), NamedValue("1295mV", 1295), NamedValue("1300mV", 1300),
            NamedValue("1305mV", 1305), NamedValue("1310mV", 1310), NamedValue("1315mV", 1315),
            NamedValue("1320mV", 1320), NamedValue("1325mV", 1325), NamedValue("1330mV", 1330),
            NamedValue("1335mV", 1335), NamedValue("1340mV", 1340), NamedValue("1345mV", 1345),
            NamedValue("1350mV", 1350), NamedValue("1355mV", 1355), NamedValue("1360mV", 1360),
            NamedValue("1365mV", 1365), NamedValue("1370mV", 1370), NamedValue("1375mV", 1375),
            NamedValue("1380mV", 1380), NamedValue("1385mV", 1385), NamedValue("1390mV", 1390),
            NamedValue("1395mV", 1395), NamedValue("1400mV", 1400), NamedValue("1405mV", 1405),
            NamedValue("1410mV", 1410), NamedValue("1415mV", 1415), NamedValue("1420mV", 1420),
            NamedValue("1425mV", 1425), NamedValue("1430mV", 1430), NamedValue("1435mV", 1435),
            NamedValue("1440mV", 1440), NamedValue("1445mV", 1445), NamedValue("1450mV", 1450),
            NamedValue("1455mV", 1455), NamedValue("1460mV", 1460), NamedValue("1465mV", 1465),
            NamedValue("1470mV", 1470), NamedValue("1475mV", 1475), NamedValue("1480mV", 1480),
            NamedValue("1485mV", 1485), NamedValue("1490mV", 1490), NamedValue("1495mV", 1495),
            NamedValue("1500mV", 1500), NamedValue("1505mV", 1505), NamedValue("1510mV", 1510),
            NamedValue("1515mV", 1515), NamedValue("1520mV", 1520), NamedValue("1525mV", 1525),
        };

        if (IsMariko()) {
            addConfigButton(KipConfigValue_g_volt_76800, "76.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_153600, "153.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_230400, "230.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_307200, "307.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_384000, "384.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_460800, "460.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_537600, "537.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_614400, "614.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_691200, "691.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_768000, "768.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_844800, "844.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_921600, "921.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_998400, "998.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_1075200, "1075.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_1152000, "1152.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_1228800, "1228.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_1267200, "1267.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_1305600, "1305.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_1344000, "1344.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1382400, "1382.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1420800, "1420.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1459200, "1459.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1497600, "1497.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1536000, "1536.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1574400, "1574.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1612800, "1612.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1651200, "1651.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1689600, "1689.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1728000, "1728.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1766400, "1766.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_1804800, "1804.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);

        } else {
            addConfigButton(KipConfigValue_g_volt_e_76800, "76.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_153600, "153.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_230400, "230.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_307200, "307.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_384000, "384.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_460800, "460.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_537600, "537.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_614400, "614.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_691200, "691.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_768000, "768.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_844800, "844.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_921600, "921.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_998400, "998.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts, false);
            addConfigButton(KipConfigValue_g_volt_e_1075200, "1075.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1152000, "1152.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1228800, "1228.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1267200, "1267.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1305600, "1305.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, eGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1344000, "1344.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1382400, "1382.4MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1420800, "1420.8MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1459200, "1459.2MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1497600, "1497.6MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &EgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
            addConfigButton(KipConfigValue_g_volt_e_1536000, "1536.0MHz", ValueRange(0, 0, 0, "0", 1), "Voltage", &MgpuVmaxThresholds, {}, mGpuVolts_noAuto, false);
        }
    }
};


static std::string getValueDisplayText(uint64_t currentValue,
                                       const ValueRange& range,
                                       const std::vector<NamedValue>& namedValues)
{
    char valueText[32];

    for (const auto& namedValue : namedValues) {
        if (currentValue == namedValue.value) {
            return namedValue.name;
        }
    }

    if (currentValue == 0) {
        snprintf(valueText, sizeof(valueText), "%s", VALUE_DEFAULT_TEXT);
    } else {
        uint64_t displayValue = currentValue / range.divisor;
        if (!range.suffix.empty()) {
            snprintf(valueText, sizeof(valueText), "%lu %s", displayValue, range.suffix.c_str());
        } else {
            snprintf(valueText, sizeof(valueText), "%lu", displayValue);
        }
    }
    return std::string(valueText);
}

void MiscGui::refresh() {
    BaseMenuGui::refresh();

    if (this->context && ++frameCounter >= 60) {
        frameCounter = 0;

        Result rc = sysclkIpcGetConfigValues(this->configList);
        if (R_FAILED(rc)) [[unlikely]] {
            FatalGui::openWithResultCode("sysclkIpcGetConfigValues", rc);
            return;
        }
        updateConfigToggles();

        for (const auto& [configVal, button] : this->configButtons) {
            uint64_t currentValue = this->configList->values[configVal];
            const ValueRange& range = this->configRanges[configVal];

            auto namedValuesIt = this->configNamedValues.find(configVal);
            const std::vector<NamedValue>& namedValues = (namedValuesIt != this->configNamedValues.end())
                ? namedValuesIt->second
                : std::vector<NamedValue>();

            char valueText[32];

            bool foundNamedValue = false;
            for (const auto& namedValue : namedValues) {
                if (currentValue == namedValue.value) {
                    snprintf(valueText, sizeof(valueText), "%s", namedValue.name.c_str());
                    foundNamedValue = true;
                    break;
                }
            }

            if (!foundNamedValue) {
                uint64_t displayValue = currentValue / range.divisor;
                if (!range.suffix.empty()) {
                    snprintf(valueText, sizeof(valueText), "%lu %s", displayValue, range.suffix.c_str());
                } else {
                    snprintf(valueText, sizeof(valueText), "%lu", displayValue);
                }
            }

            button->setValue(valueText);
        }
    }
}
