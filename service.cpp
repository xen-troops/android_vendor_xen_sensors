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

#define LOG_TAG "android.hardware.sensors@1.0-service.xenvm"

#include <android/hardware/sensors/1.0/ISensors.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>

#include "Sensors.h"

using ::android::OK;
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;
using ::android::hardware::sensors::V1_0::ISensors;
using ::android::hardware::sensors::V1_0::xenvm::Sensors;
using ::android::sp;

int main(int /* argc */, char* /* argv */ []) {
    sp<ISensors> service = new Sensors();
    /* Sensors framework service needs at least two threads.
     * One thread blocks on a "poll"
     * The second thread is needed for all other HAL methods.
     * During VTS test we found that 4 is the most suitable thread count.
     */
    configureRpcThreadpool(4, true /* will join */);
    if (service->registerAsService() != OK) {
        ALOGE("Could not register sensors.xenvm 1.0 service.");
        return 1;
    }
    joinRpcThreadpool();

    ALOGE("Service exited!");
    return 1;
}
