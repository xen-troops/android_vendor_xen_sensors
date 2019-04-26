/*
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

#define LOG_TAG "sensors@1.0-xenvm.factory"

#include "SensorsFactory.h"

#include <android-base/macros.h>
#include <android/log.h>
#include <cutils/properties.h>
#include <log/log.h>

#include <fstream>

#undef TRY_AGAIN

#include <json/reader.h>

bool SensorsFactory::buildSensorsFromConfig(std::vector<ExtendedSensorInfo>& sensorList,
                                            std::map<std::string, int32_t>& visToSensorHandle,
                                            std::map<int32_t, std::string>& sensorHandleToVis) {
    char propValue[PROPERTY_VALUE_MAX] = {};
    property_get("persist.sensors.config", propValue, "/vendor/etc/vehicle/sensors-config.json");
    std::ifstream configFile(propValue);
    if (configFile.is_open()) {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(configFile, root, false)) {
            if (root.isArray()) {
                for (unsigned int i = 0; i < root.size(); i++) {
                    Json::Value val = root[i];
                    const std::string& sensorName = val[ksSensorName].asString();
                    const std::string& sensorVendor = val[ksSensorVendor].asString();
                    int version = val[ksVersion].asInt();
                    int type = val[ksSensorType].asInt();
                    const std::string& typeString = val[ksSensorTypeString].asString();
                    float maxRange = val[ksMaxRange].asFloat();
                    float resolution = val[ksResolution].asFloat();
                    float power = val[ksPower].asFloat();
                    int minDelay = val[ksMinDelay].asInt();
                    int maxDelay = val[ksMaxDelay].asInt();
                    const std::string& permission = val[ksPermission].asString();
                    unsigned int flags = val[ksFlags].asUInt();
                    const std::string& visProperty = val[ksVisProperty].asString();
                    ExtendedSensorInfo esi = {
                        .si = {.sensorHandle = static_cast<int32_t>(sensorList.size()),
                               .name = sensorName,
                               .vendor = sensorVendor,
                               .version = version,
                               .type = static_cast<SensorType>(type),
                               .typeAsString = typeString,
                               .maxRange = maxRange,
                               .resolution = resolution,
                               .power = power,
                               .minDelay = minDelay,
                               .maxDelay = maxDelay,
                               .requiredPermission = permission,
                               .flags = flags},
                        .visPropertyName = visProperty,
                        .enabled = false,
                    };
                    sensorList.push_back(esi);
                    visToSensorHandle.emplace(visProperty, esi.si.sensorHandle);
                    sensorHandleToVis.emplace(esi.si.sensorHandle, visProperty);
                    ALOGD("Creating sensor info name=%s flags=%u", sensorName.c_str(), flags);
                }
            }
        } else {
            ALOGE("Error parsing sensor config: %s", reader.getFormatedErrorMessages().c_str());
            return false;
        }

    } else {
        ALOGE("Unable to open sensor config file %s", propValue);
        return false;
    }
    return true;
}
