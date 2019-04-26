/*
 * Copyright (C) 2016 The Android Open Source Project
 * Copyright (C) 2019 EPAM systems
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SENSORS_FACTORY_H_

#define SENSORS_FACTORY_H_

#include <android/hardware/sensors/1.0/ISensors.h>
#include <android/hardware/sensors/1.0/types.h>
#include <hardware/sensors.h>

#include <map>
#include <string>
#include <vector>

#include "Sensors.h"

using android::hardware::hidl_vec;
using android::hardware::sensors::V1_0::SensorFlagBits;
using android::hardware::sensors::V1_0::SensorInfo;
using android::hardware::sensors::V1_0::SensorType;
using android::hardware::sensors::V1_0::xenvm::ExtendedSensorInfo;

class SensorsFactory {
    SensorsFactory() = delete;
    ~SensorsFactory() = delete;

 public:
    static bool buildSensorsFromConfig(std::vector<ExtendedSensorInfo>& sensorList,
                                       std::map<std::string, int32_t>& visToSensorHandle,
                                       std::map<int32_t, std::string>& sensorHandleToVis);

 private:
    static constexpr const char* ksSensorName = "sensor-name";
    static constexpr const char* ksSensorVendor = "sensor-vendor";
    static constexpr const char* ksVersion = "version";
    static constexpr const char* ksSensorType = "sensor-type";
    static constexpr const char* ksSensorTypeString = "sensor-type-string";
    static constexpr const char* ksMaxRange = "max-range";
    static constexpr const char* ksResolution = "resolution";
    static constexpr const char* ksPower = "power";
    static constexpr const char* ksMinDelay = "min-delay";
    static constexpr const char* ksMaxDelay = "max-delay";
    static constexpr const char* ksPermission = "permission";
    static constexpr const char* ksFlags = "flags";
    static constexpr const char* ksVisProperty = "vis-property";
};

#endif  // SENSORS_FACTORY_H_
