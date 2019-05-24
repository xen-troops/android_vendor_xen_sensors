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

// #define LOG_NDEBUG 0
// #define GENERATE_FAKE_EVENTS
#define LOG_TAG "sensors@1.0-xenvm"

#include "Sensors.h"
#include "SensorsFactory.h"

#include <log/log.h>
#include <sys/stat.h>
#include <utils/SystemClock.h>

#include <algorithm>

namespace android {
namespace hardware {
namespace sensors {
namespace V1_0 {
namespace xenvm {

/* VIS related */

// TODO(Andrii): Add subscribeExisting
void Sensors::subscribeToAll() {
    // Subscribe to all
    ALOGV("Will try to subscribe to all VIS properties");
    epam::WMessageResult result;
    std::function<void(const epam::CommandResult&)> resultHandler =
        std::bind(&Sensors::subscriptionHandler, this, std::placeholders::_1);
    epam::Status st = mVisClient.subscribePropertySync(VisClient::kAllTag, resultHandler, result);

    if (st == epam::Status::OK) {
        mMainSubscriptionId = result.subscriptionId;
        ALOGI("Subscribed with id =%zu", result.subscriptionId);
        mSubscribed = true;
    }
}

void Sensors::subscriptionHandler(const epam::CommandResult& result) {
    ALOGV("Sensors::subscriptionHandler");
    for (auto r : result) {
        ALOGV("Sensors::subscriptionHandler processing {%s}", r.first.c_str());
        auto sit = mVisToSensorHandle.find(r.first);
        if (sit != mVisToSensorHandle.end()) {
            ALOGV("Found updated property [%s] for sensor handle = %d", r.first.c_str(),
                  sit->second);
            if (convertJsonToSensorPayload(sit->second, r.second)) {
                if (!isContiniousSensor(mSensorList[sit->second].si.type)) {
                    std::lock_guard<std::mutex> lock(mEventLock);
                    Event ev = {
                        .sensorHandle = sit->second,
                        .sensorType =
                            reinterpret_cast<SensorType>(mSensorList[sit->second].si.type),
                        .timestamp = elapsedRealtimeNano(),
                        .u = mSensorList[sit->second].eventPayload,
                    };
                    insertEventUnlocked(ev);
                }
            }
        }
    }
}

bool Sensors::convertJsonToSensorPayload(int32_t sensorHandle, Json::Value& jval) {
    switch ((SensorType)mSensorList[sensorHandle].si.type) {
        case SensorType::ACCELEROMETER:
        case SensorType::MAGNETIC_FIELD:
        case SensorType::ORIENTATION:
        case SensorType::GYROSCOPE:
        case SensorType::GRAVITY:
        case SensorType::LINEAR_ACCELERATION: {
            for (int i = 0; i < 3; i++) {
                if (!jval[i].isConvertibleTo(Json::realValue)) {
                    ALOGE("Can't convert from JSON to float data for sensor handle %d",
                          sensorHandle);
                    return false;
                }
            }
            if (!jval[3].isConvertibleTo(Json::intValue)) {
                ALOGE("Can't convert from JSON to sensor status for sensor handle %d",
                      sensorHandle);
                return false;
            }
            mSensorList[sensorHandle].eventPayload.vec3.x = jval[0].asFloat();
            mSensorList[sensorHandle].eventPayload.vec3.y = jval[1].asFloat();
            mSensorList[sensorHandle].eventPayload.vec3.z = jval[2].asFloat();
            int status = jval[3].asInt();
            mSensorList[sensorHandle].eventPayload.vec3.status =
                (status > static_cast<int>(SensorStatus::ACCURACY_HIGH) ||
                 status < static_cast<int>(SensorStatus::NO_CONTACT))
                    ? SensorStatus::UNRELIABLE
                    : static_cast<SensorStatus>(status);
            return true;
        }
        case SensorType::GAME_ROTATION_VECTOR: {
            for (int i = 0; i < 4; i++) {
                if (!jval[i].isConvertibleTo(Json::realValue)) {
                    ALOGE("Can't convert from JSON to float data for sensor handle %d",
                          sensorHandle);
                    return false;
                }
            }
            mSensorList[sensorHandle].eventPayload.vec4.x = jval[0].asFloat();
            mSensorList[sensorHandle].eventPayload.vec4.y = jval[1].asFloat();
            mSensorList[sensorHandle].eventPayload.vec4.z = jval[2].asFloat();
            mSensorList[sensorHandle].eventPayload.vec4.w = jval[3].asFloat();
            return true;
        }
        case SensorType::ROTATION_VECTOR:
        case SensorType::GEOMAGNETIC_ROTATION_VECTOR: {
            for (int i = 0; i < 5; i++) {
                if (!jval[i].isConvertibleTo(Json::realValue)) {
                    ALOGE("Can't convert from JSON to float data for sensor handle %d",
                          sensorHandle);
                    return false;
                }
                mSensorList[sensorHandle].eventPayload.data[i] = jval[i].asFloat();
            }
            return true;
        }
        case SensorType::MAGNETIC_FIELD_UNCALIBRATED:
        case SensorType::GYROSCOPE_UNCALIBRATED:
        case SensorType::ACCELEROMETER_UNCALIBRATED: {
            for (int i = 0; i < 6; i++) {
                if (!jval[i].isConvertibleTo(Json::realValue)) {
                    ALOGE("Can't convert from JSON to float data for sensor handle %d",
                          sensorHandle);
                    return false;
                }
                mSensorList[sensorHandle].eventPayload.data[i] = jval[i].asFloat();
            }
            mSensorList[sensorHandle].eventPayload.uncal.x = jval[0].asFloat();
            mSensorList[sensorHandle].eventPayload.uncal.y = jval[1].asFloat();
            mSensorList[sensorHandle].eventPayload.uncal.z = jval[2].asFloat();
            mSensorList[sensorHandle].eventPayload.uncal.x_bias = jval[3].asFloat();
            mSensorList[sensorHandle].eventPayload.uncal.y_bias = jval[4].asFloat();
            mSensorList[sensorHandle].eventPayload.uncal.z_bias = jval[5].asFloat();
            return true;
        }
        case SensorType::DEVICE_ORIENTATION:
        case SensorType::LIGHT:
        case SensorType::PRESSURE:
        case SensorType::TEMPERATURE:
        case SensorType::PROXIMITY:
        case SensorType::RELATIVE_HUMIDITY:
        case SensorType::AMBIENT_TEMPERATURE:
        case SensorType::SIGNIFICANT_MOTION:
        case SensorType::STEP_DETECTOR:
        case SensorType::TILT_DETECTOR:
        case SensorType::WAKE_GESTURE:
        case SensorType::GLANCE_GESTURE:
        case SensorType::PICK_UP_GESTURE:
        case SensorType::WRIST_TILT_GESTURE:
        case SensorType::STATIONARY_DETECT:
        case SensorType::MOTION_DETECT:
        case SensorType::HEART_BEAT:
        case SensorType::LOW_LATENCY_OFFBODY_DETECT: {
            if (!jval.isConvertibleTo(Json::realValue)) {
                ALOGE("Can't convert from JSON to float data for sensor handle %d", sensorHandle);
                return false;
            }
            mSensorList[sensorHandle].eventPayload.scalar = jval.asFloat();
            return true;
        }
        case SensorType::STEP_COUNTER: {
            if (!jval.isUInt64()) {
                ALOGE("Can't convert from JSON to isUInt64 data for sensor handle %d",
                      sensorHandle);
                return false;
            }
            mSensorList[sensorHandle].eventPayload.stepCount = jval.isUInt64();
            return true;
        }
        case SensorType::HEART_RATE: {
            if (!jval[0].isConvertibleTo(Json::realValue)) {
                ALOGE("Can't convert from JSON to float data for sensor handle %d", sensorHandle);
                return false;
            }
            if (!jval[1].isConvertibleTo(Json::intValue)) {
                ALOGE("Can't convert from JSON to sensor status for sensor handle %d",
                      sensorHandle);
                return false;
            }
            mSensorList[sensorHandle].eventPayload.heartRate.bpm = jval[0].asFloat();
            int status = jval[1].asInt();
            mSensorList[sensorHandle].eventPayload.heartRate.status =
                (status > static_cast<int>(SensorStatus::ACCURACY_HIGH) ||
                 status < static_cast<int>(SensorStatus::NO_CONTACT))
                    ? SensorStatus::UNRELIABLE
                    : static_cast<SensorStatus>(status);
            return true;
        }
        case SensorType::POSE_6DOF: {  // 15 floats
            for (int i = 0; i < 15; ++i) {
                if (!jval[i].isConvertibleTo(Json::realValue)) {
                    ALOGE("Can't convert from JSON to float data for sensor handle %d",
                          sensorHandle);
                    return false;
                }
                mSensorList[sensorHandle].eventPayload.pose6DOF[i] = jval[i].asFloat();
            }
            return true;
        }
        default:
            ALOGE("Can't convert, unknown sensor type for handle %d", sensorHandle);
            return false;
    }
    return false;
}

void Sensors::handleConnectionAsync(bool connected) {
    std::lock_guard<std::mutex> lock(mLock);
    ALOGI("Received connection state update to %d from VisClient", connected);
    mVisConnceted = connected;
    if (mVisConnceted) {
        ALOGI("Connected to VIS...");
        epam::WMessageResult sr;
        epam::Status st = mVisClient.getPropertySync(VisClient::kAllTag, sr);
        if (st != epam::Status::OK) {
            ALOGE("Unable to get * from VIS");
            return;
        }
        ALOGV("Get * from VIS.");
        subscriptionHandler(sr.commandResult);
        subscribeToAll();
    } else {
        ALOGI("Disconnected from VIS...");
    }
}

void Sensors::onVisConnectionStatusUpdate(bool connected) {
    ALOGI("onVisConnectionStatusUpdate: Received connection state update to %d from VisClient",
          connected);
    // Async c-back to reach out of VisClient main thread context
    std::function<void(bool)> resultHandler =
        std::bind(&Sensors::handleConnectionAsync, this, std::placeholders::_1);
    mConnectionFuture = std::async(std::launch::async, resultHandler, connected);
}

/* General sensors related */

Sensors::Sensors()
    : mInitCheck(NO_INIT),
      mRecurrentTimer(std::bind(&Sensors::onContinuousPropertyTimer, this, std::placeholders::_1)) {
    ALOGD("Init");
    mActiveSensors = 0;
    if (SensorsFactory::buildSensorsFromConfig(mSensorList, mVisToSensorHandle,
                                               sensorHandleToVis)) {
        mInitCheck = OK;
        ALOGD("Parced sensor config...");
        mOperationMode = OperationMode::NORMAL;
        std::function<void(bool)> connHandler =
            std::bind(&Sensors::onVisConnectionStatusUpdate, this, std::placeholders::_1);
        mVisClient.registerServerConnectionhandler(connHandler);
        mVisClient.start();
        ALOGD("Started VIS client...");
    } else {
        mInitCheck = UNKNOWN_ERROR;
        ALOGE("Failed to build sensors from config....");
    }
    // TODO(Andrii): Add static configuration
}

Sensors::~Sensors() { mVisClient.stop(); }

void Sensors::insertEventUnlocked(Event& ev) {
    mEventQueue.push(ev);
    if (mEventQueue.size() > kMaxEventQueueSize) {
        mEventQueue.pop();
        ALOGE("Event queue overflow, dropped oldest event");
    }
    mPollCondition.notify_all();
    ALOGV("notified all about event...");
}

void Sensors::onContinuousPropertyTimer(const std::vector<int32_t>& properties) {
    for (int32_t property : properties) {
        if (isContiniousSensor(mSensorList[property].si.type)) {
            std::lock_guard<std::mutex> lock(mEventLock);
            Event ev;
#ifdef GENERATE_FAKE_EVENTS
            ev = createFakeEvent(property);
#else
            ev = {
                .sensorHandle = property,
                .sensorType = reinterpret_cast<SensorType>(mSensorList[property].si.type),
                .timestamp = elapsedRealtimeNano(),
                .u = mSensorList[property].eventPayload,
            };
#endif
            insertEventUnlocked(ev);
            ALOGV("Recurrent event for sensor handle %d", property);
        } else {
            ALOGE("Unexpected onContinuousPropertyTimer for property: 0x%x", property);
        }
    }
}

status_t Sensors::initCheck() const { return mInitCheck; }

/*
union EventPayload final {
    ::android::hardware::sensors::V1_0::Vec3 vec3 __attribute__ ((aligned(4)));
    ::android::hardware::sensors::V1_0::Vec4 vec4 __attribute__ ((aligned(4)));
    ::android::hardware::sensors::V1_0::Uncal uncal __attribute__ ((aligned(4)));
    ::android::hardware::sensors::V1_0::MetaData meta __attribute__ ((aligned(4)));
    float scalar __attribute__ ((aligned(4)));
    uint64_t stepCount __attribute__ ((aligned(8)));
    ::android::hardware::sensors::V1_0::HeartRate heartRate __attribute__ ((aligned(4)));
    ::android::hardware::hidl_array<float, 15> pose6DOF __attribute__ ((aligned(4)));
    ::android::hardware::sensors::V1_0::DynamicSensorInfo dynamic __attribute__ ((aligned(4)));
    ::android::hardware::sensors::V1_0::AdditionalInfo additional __attribute__ ((aligned(4)));
    ::android::hardware::hidl_array<float, 16> data __attribute__ ((aligned(4)));
};
*/

Event Sensors::createFakeEvent(int32_t sensor_handle) {
    static float delta = 0.1;
    Event ev = {
        .sensorHandle = sensor_handle,
        .sensorType = (SensorType)mSensorList[sensor_handle].si.type,
        .timestamp = elapsedRealtimeNano(),
    };

    switch (ev.sensorType) {
            /*
            struct Vec3 final {
            float x __attribute__ ((aligned(4)));
            float y __attribute__ ((aligned(4)));
            float z __attribute__ ((aligned(4)));
            ::android::hardware::sensors::V1_0::SensorStatus status __attribute__ ((aligned(1)));
            };
            */
        case SensorType::ACCELEROMETER:
        case SensorType::MAGNETIC_FIELD:
        case SensorType::ORIENTATION:
        case SensorType::GYROSCOPE:
        case SensorType::GRAVITY:
        case SensorType::LINEAR_ACCELERATION: {
            ev.u.vec3.x = 6.3 + delta;
            ev.u.vec3.y = 6.2;
            ev.u.vec3.z = 6.1 - delta;
            ev.u.vec3.status = SensorStatus::ACCURACY_HIGH;
            delta = delta * 2;
            if (delta > 0.8) delta = 0.1;
            break;
        }

            /*
            struct Vec4 final {
                float x __attribute__ ((aligned(4)));
                float y __attribute__ ((aligned(4)));
                float z __attribute__ ((aligned(4)));
                float w __attribute__ ((aligned(4)));
            };
            */
        case SensorType::GAME_ROTATION_VECTOR: {
            ev.u.vec4.x = 1;
            ev.u.vec4.y = 2;
            ev.u.vec4.z = 3;
            ev.u.vec4.w = 4;
            break;
        }
            /*
            ::android::hardware::hidl_array<float, 16> data __attribute__ ((aligned(4)));
            */
        case SensorType::ROTATION_VECTOR:
        case SensorType::GEOMAGNETIC_ROTATION_VECTOR: {
            ev.u.data[0] = 0;
            ev.u.data[1] = 1;
            ev.u.data[2] = 2;
            ev.u.data[3] = 3;
            ev.u.data[4] = 4;
            break;
        }
            /*
            struct Uncal final {
                float x __attribute__ ((aligned(4)));
                float y __attribute__ ((aligned(4)));
                float z __attribute__ ((aligned(4)));
                float x_bias __attribute__ ((aligned(4)));
                float y_bias __attribute__ ((aligned(4)));
                float z_bias __attribute__ ((aligned(4)));
            };
            */
        case SensorType::MAGNETIC_FIELD_UNCALIBRATED:
        case SensorType::GYROSCOPE_UNCALIBRATED:
        case SensorType::ACCELEROMETER_UNCALIBRATED: {
            ev.u.uncal.x = 0.1;
            ev.u.uncal.y = 0.2;
            ev.u.uncal.z = 0.3;
            ev.u.uncal.x_bias = 0.3;
            ev.u.uncal.y_bias = 0.2;
            ev.u.uncal.z_bias = 0.1;
            break;
        }

            /*
            float scalar __attribute__ ((aligned(4)));
            */
        case SensorType::DEVICE_ORIENTATION:
        case SensorType::LIGHT:
        case SensorType::PRESSURE:
        case SensorType::TEMPERATURE:
        case SensorType::PROXIMITY:
        case SensorType::RELATIVE_HUMIDITY:
        case SensorType::AMBIENT_TEMPERATURE:
        case SensorType::SIGNIFICANT_MOTION:
        case SensorType::STEP_DETECTOR:
        case SensorType::TILT_DETECTOR:
        case SensorType::WAKE_GESTURE:
        case SensorType::GLANCE_GESTURE:
        case SensorType::PICK_UP_GESTURE:
        case SensorType::WRIST_TILT_GESTURE:
        case SensorType::STATIONARY_DETECT:
        case SensorType::MOTION_DETECT:
        case SensorType::HEART_BEAT:
        case SensorType::LOW_LATENCY_OFFBODY_DETECT: {
            ev.u.scalar = 3.4;
            break;
        }

            /*
            uint64_t stepCount __attribute__ ((aligned(8)));
            */
        case SensorType::STEP_COUNTER: {
            ev.u.stepCount = 1024;
            break;
        }
            /*
            struct HeartRate final {
                float bpm __attribute__ ((aligned(4)));
                ::android::hardware::sensors::V1_0::SensorStatus status __attribute__
            ((aligned(1)));
            };
            */
        case SensorType::HEART_RATE: {
            ev.u.heartRate.bpm = 1024.6;
            ev.u.heartRate.status = SensorStatus::ACCURACY_HIGH;
            break;
        }

        case SensorType::POSE_6DOF: {  // 15 floats
            for (size_t i = 0; i < 15; ++i) {
                ev.u.pose6DOF[i] = 14.4;
            }
            break;
        }
        default:
            ALOGE("Can't create fake event");
    }
    return ev;
}

/* Public Sensors HAL API */

Return<void> Sensors::getSensorsList(getSensorsList_cb _hidl_cb) {
    ALOGD("getSensorsList");
    hidl_vec<SensorInfo> out;

    {
        std::lock_guard<std::mutex> lock(mLock);
        size_t count = mSensorList.size();
        out.resize(count);

        for (size_t i = 0; i < count; ++i) {
            out[i] = mSensorList[i].si;
        }
    }

    _hidl_cb(out);
    return Void();
}

Return<Result> Sensors::setOperationMode(OperationMode mode) {
    ALOGD("setOperationMode mode=%d", static_cast<int32_t>(mode));
    std::lock_guard<std::mutex> lock(mLock);
    if (mode == OperationMode::DATA_INJECTION) return Result::BAD_VALUE;
    mOperationMode = mode;
    return Result::OK;
}

Return<Result> Sensors::activate(int32_t sensor_handle, bool enabled) {
    ALOGV("activate sensor_handle=%d enabled=%d", sensor_handle, enabled);
    std::lock_guard<std::mutex> lock(mLock);
    if (mSensorList[sensor_handle].enabled != enabled) {
        enabled ? ++mActiveSensors : --mActiveSensors;
        mSensorList[sensor_handle].enabled = enabled;
        if (isRecurrentEventNeeded(mSensorList[sensor_handle].si.type) &&
            mSensorList[sensor_handle].samplingPeriodNs != 0) {
            if (mSensorList[sensor_handle].enabled) {
                ALOGV("Scheduled reccurent event using =%lu",mSensorList[sensor_handle].samplingPeriodNs);
                mRecurrentTimer.registerRecurrentEvent(std::chrono::nanoseconds{mSensorList[sensor_handle].samplingPeriodNs}, sensor_handle);
            } else {
                ALOGV(" Unregister reccurent event for sensor handle %d", sensor_handle);
                mRecurrentTimer.unregisterRecurrentEvent(sensor_handle);
            }
        }
    }
    return Result::OK;
}

Return<void> Sensors::poll(int32_t maxCount, poll_cb _hidl_cb) {
    ALOGV("poll maxCount=%d", maxCount);
    hidl_vec<Event> out;
    hidl_vec<SensorInfo> dynamicSensorsAdded;


    if (!maxCount) {
        _hidl_cb(Result::OK, out, dynamicSensorsAdded);
        ALOGV("Sent events %zu dyn sensors added %zu", out.size(), dynamicSensorsAdded.size());
        return Void();
    }

    std::unique_lock<std::mutex> lock(mPollLock);
    size_t eventCnt = maxCount;

    while (1) {
        std::unique_lock<std::mutex> ulock(mPollConditionLock);
        {
            std::unique_lock<std::mutex> elock(mEventLock);
            if ((mEventQueue.size() > 0)) {
                if (eventCnt >= mEventQueue.size()) eventCnt = mEventQueue.size();
                out.resize(eventCnt);
                for (size_t i = 0; i < eventCnt; i++) {
                    out[i] = mEventQueue.front();
                    mEventQueue.pop();
                }
                break;
            }
        }
        mPollCondition.wait_for(ulock, std::chrono::milliseconds(500));
        ALOGV("got notify about events...");
    }
    _hidl_cb(Result::OK, out, dynamicSensorsAdded);
    ALOGV("Sent events %zu dyn sensors added %zu", out.size(), dynamicSensorsAdded.size());
    return Void();
}

Return<Result> Sensors::batch(int32_t sensor_handle, int64_t sampling_period_ns,
                              int64_t max_report_latency_ns) {
    ALOGV("batch sensor_handle=%d sampling_period_ns=%ld max_report_latency_ns=%ld", sensor_handle,
          sampling_period_ns, max_report_latency_ns);
    std::lock_guard<std::mutex> lock(mLock);
    if (isRecurrentEventNeeded(mSensorList[sensor_handle].si.type)) {
        if (mSensorList[sensor_handle].enabled) {
            if (mSensorList[sensor_handle].samplingPeriodNs != 0) {
                ALOGV("Unregister reccurent event for sensor handle %d", sensor_handle);
                mRecurrentTimer.unregisterRecurrentEvent(sensor_handle);
            }
            ALOGV("Scheduled reccurent event using =%lu for sensor handle %d",mSensorList[sensor_handle].samplingPeriodNs, sensor_handle);
            mRecurrentTimer.registerRecurrentEvent(std::chrono::nanoseconds{sampling_period_ns}, sensor_handle);
        }
        mSensorList[sensor_handle].samplingPeriodNs = sampling_period_ns;
        mSensorList[sensor_handle].maxReportLatencyNs = max_report_latency_ns;
        ALOGV("Sensor[%d] sampling_period_ns=%ld max_report_latency_ns=%ld were updated",
              sensor_handle, sampling_period_ns, max_report_latency_ns);
        return Result::OK;
    }

    return Result::INVALID_OPERATION;
}

Return<Result> Sensors::flush(int32_t sensor_handle) {
    ALOGV("flush sensor_handle=%d", sensor_handle);
    std::lock_guard<std::mutex> lock(mEventLock);
    Event ev = {
        .sensorHandle = sensor_handle,
        .sensorType = SensorType::META_DATA,
        .timestamp = 0ll,
    };
    ev.u.meta.what = MetaDataEventType::META_DATA_FLUSH_COMPLETE;
    insertEventUnlocked(ev);
    return Result::OK;
}

Return<Result> Sensors::injectSensorData(const Event& /*event*/) {
    ALOGV("injectSensorData");
    // HAL does not support
    return Result::INVALID_OPERATION;
}

Return<void> Sensors::registerDirectChannel(const SharedMemInfo& /*mem*/,
                                            registerDirectChannel_cb _hidl_cb) {
    ALOGV("registerDirectChannel");
    // HAL does not support
    _hidl_cb(Result::INVALID_OPERATION, -1);
    return Void();
}

Return<Result> Sensors::unregisterDirectChannel(int32_t channelHandle) {
    ALOGV("unregisterDirectChannel %d", channelHandle);
    // HAL does not support
    return Result::INVALID_OPERATION;
}

Return<void> Sensors::configDirectReport(int32_t /*sensorHandle*/, int32_t /*channelHandle*/,
                                         RateLevel /*rate*/, configDirectReport_cb _hidl_cb) {
    // HAL does not support
    _hidl_cb(Result::INVALID_OPERATION, -1);
    return Void();
}

bool Sensors::isOneShotSensor(SensorType sType) const {
    switch (sType) {
        case SensorType::SIGNIFICANT_MOTION:
        case SensorType::WAKE_GESTURE:
        case SensorType::GLANCE_GESTURE:
        case SensorType::DEVICE_ORIENTATION:
        case SensorType::STATIONARY_DETECT:
        case SensorType::MOTION_DETECT:
            return true;
        default:
            return false;
    }
    return false;
}

bool Sensors::isContiniousSensor(SensorType sType) const {
    switch (sType) {
        case SensorType::ACCELEROMETER:
        case SensorType::MAGNETIC_FIELD:
        case SensorType::ORIENTATION:
        case SensorType::GYROSCOPE:
        case SensorType::PRESSURE:
        case SensorType::GRAVITY:
        case SensorType::LINEAR_ACCELERATION:
        case SensorType::ROTATION_VECTOR:
        case SensorType::MAGNETIC_FIELD_UNCALIBRATED:
        case SensorType::GAME_ROTATION_VECTOR:
        case SensorType::GYROSCOPE_UNCALIBRATED:
        case SensorType::GEOMAGNETIC_ROTATION_VECTOR:
        case SensorType::POSE_6DOF:
        case SensorType::HEART_BEAT:
        case SensorType::ACCELEROMETER_UNCALIBRATED:
            return true;
        default:
            return false;
    }
    return false;
}

bool Sensors::isOnChangeSensor(SensorType sType) const {
    switch (sType) {
        case SensorType::LIGHT:
        case SensorType::PROXIMITY:
        case SensorType::RELATIVE_HUMIDITY:
        case SensorType::AMBIENT_TEMPERATURE:
        case SensorType::STEP_COUNTER:
        case SensorType::HEART_RATE:
        case SensorType::DEVICE_ORIENTATION:
        case SensorType::LOW_LATENCY_OFFBODY_DETECT:
            return true;
        default:
            return false;
    }
    return false;
}

// static

ISensors* HIDL_FETCH_ISensors(const char* /* hal */) {
    Sensors* sensors = new Sensors;
    if (sensors->initCheck() != OK) {
        delete sensors;
        sensors = nullptr;

        return nullptr;
    }

    return sensors;
}

}  // namespace xenvm
}  // namespace V1_0
}  // namespace sensors
}  // namespace hardware
}  // namespace android
