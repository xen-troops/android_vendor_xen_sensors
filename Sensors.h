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

#ifndef SENSORS_H_

#define SENSORS_H_

#include <android-base/macros.h>
#include <android/hardware/sensors/1.0/ISensors.h>
#include <hardware/sensors.h>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "RecurrentTimer.h"
#include "VisClient.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V1_0 {
namespace xenvm {

using android::hardware::sensors::V1_0::OperationMode;
using android::hardware::sensors::V1_0::SensorType;
using epam::VisClient;

struct ExtendedSensorInfo {
    SensorInfo si;
    std::string visPropertyName;
    bool enabled;
    int64_t samplingPeriodNs;
    int64_t maxReportLatencyNs;
    EventPayload eventPayload;
};

struct Sensors : public ::android::hardware::sensors::V1_0::ISensors {
    Sensors();
    ~Sensors();

    status_t initCheck() const;

    Return<void> getSensorsList(getSensorsList_cb _hidl_cb) override;

    Return<Result> setOperationMode(OperationMode mode) override;

    Return<Result> activate(
            int32_t sensor_handle, bool enabled) override;

    Return<void> poll(int32_t maxCount, poll_cb _hidl_cb) override;

    Return<Result> batch(
            int32_t sensor_handle,
            int64_t sampling_period_ns,
            int64_t max_report_latency_ns) override;

    Return<Result> flush(int32_t sensor_handle) override;

    Return<Result> injectSensorData(const Event& event) override;

    Return<void> registerDirectChannel(
            const SharedMemInfo& mem, registerDirectChannel_cb _hidl_cb) override;

    Return<Result> unregisterDirectChannel(int32_t channelHandle) override;

    Return<void> configDirectReport(
            int32_t sensorHandle, int32_t channelHandle, RateLevel rate,
            configDirectReport_cb _hidl_cb) override;

 private:
    static constexpr int32_t kPollMaxBufferSize = 128;
    status_t mInitCheck;
    std::mutex mPollLock;

    std::mutex mEventLock;
    std::queue<Event> mEventQueue;
    std::vector<ExtendedSensorInfo> mSensorList;
    std::map<std::string, int32_t> mVisToSensorHandle;
    std::map<int32_t, std::string> sensorHandleToVis;
    OperationMode mOperationMode;
    std::mutex mLock;
    RecurrentTimer mRecurrentTimer;
    std::mutex mPollConditionLock;
    std::condition_variable mPollCondition;

    VisClient mVisClient;
    bool mVisConnceted;
    std::future<void> mConnectionFuture;
    uint32_t mActiveSensors;
    uint32_t mMainSubscriptionId;
    bool mSubscribed;

    void onContinuousPropertyTimer(const std::vector<int32_t>& properties);
    Event createFakeEvent(int32_t sensor_handle);

    static void convertFromSensorEvents(size_t count, const sensors_event_t* src,
                                        hidl_vec<Event>* dst);
    bool isOneShotSensor(SensorType sType) const;
    bool isContiniousSensor(SensorType sType) const;
    bool isOnChangeSensor(SensorType sType) const;

    bool isRecurrentEventNeeded(SensorType sType) const {
        if (isContiniousSensor(sType) || isOnChangeSensor(sType)) return true;
        return false;
    }
    void insertEventUnlocked(Event& ev);
    void subscribeToAll();
    void subscriptionHandler(const epam::CommandResult& result);
    void onVisConnectionStatusUpdate(bool connected);
    void handleConnectionAsync(bool connected);
    bool convertJsonToSensorPayload(int32_t sensorHandle, Json::Value& val);

    static const int kMaxEventQueueSize = 10000;

    DISALLOW_COPY_AND_ASSIGN(Sensors);
};

extern "C" ISensors* HIDL_FETCH_ISensors(const char* name);

}  // namespace xenvm
}  // namespace V1_0
}  // namespace sensors
}  // namespace hardware
}  // namespace android

#endif  // SENSORS_H_
