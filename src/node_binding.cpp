#include "node_binding.hpp"
#include <map>

Napi::FunctionReference NodeCamera::constructor;

Napi::Object NodeCamera::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Camera", {
        InstanceMethod("start", &NodeCamera::Start),
        InstanceMethod("stop", &NodeCamera::Stop),
        InstanceMethod("setControls", &NodeCamera::SetControls),
        InstanceMethod("getControls", &NodeCamera::GetControls),
        InstanceMethod("getCapabilities", &NodeCamera::GetCapabilities),
        InstanceMethod("on", &NodeCamera::On),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Camera", func);

    // Export control enum constants
    const auto controls = Napi::Object::New(env);

    const auto exposureMode = Napi::Object::New(env);
    exposureMode.Set("NORMAL", 0);
    exposureMode.Set("SHORT", 1);
    exposureMode.Set("LONG", 2);
    exposureMode.Set("CUSTOM", 3);
    controls.Set("ExposureMode", exposureMode);

    auto afMode = Napi::Object::New(env);
    afMode.Set("MANUAL", 0);
    afMode.Set("AUTO", 1);
    afMode.Set("CONTINUOUS", 2);
    controls.Set("AfMode", afMode);

    const auto afTrigger = Napi::Object::New(env);
    afTrigger.Set("START", 0);
    afTrigger.Set("CANCEL", 1);
    controls.Set("AfTrigger", afTrigger);

    const auto awbMode = Napi::Object::New(env);
    awbMode.Set("AUTO", 0);
    awbMode.Set("INCANDESCENT", 1);
    awbMode.Set("TUNGSTEN", 2);
    awbMode.Set("FLUORESCENT", 3);
    awbMode.Set("INDOOR", 4);
    awbMode.Set("DAYLIGHT", 5);
    awbMode.Set("CLOUDY", 6);
    awbMode.Set("CUSTOM", 7);
    controls.Set("AwbMode", awbMode);

    exports.Set("controls", controls);

    return exports;
}

NodeCamera::NodeCamera(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<NodeCamera>(info) {
    Napi::Env env = info.Env();

    if (!info[0].IsObject()) {
        Napi::TypeError::New(env, "Configuration object expected").ThrowAsJavaScriptException();
        return;
    }

    auto config = info[0].As<Napi::Object>();
    lcam::CameraConfig cameraConfig;

    // Parse optional RAW stream configuration
    if (config.Has("rawStream")) {
        auto rawStreamObj = config.Get("rawStream").As<Napi::Object>();
        lcam::StreamConfig rawStream;
        rawStream.type = lcam::StreamType::RAW;

        if (rawStreamObj.Has("width")) {
            rawStream.width = rawStreamObj.Get("width").As<Napi::Number>().Uint32Value();
        } else {
            rawStream.width = 2304;  // Default sensor resolution
        }

        if (rawStreamObj.Has("height")) {
            rawStream.height = rawStreamObj.Get("height").As<Napi::Number>().Uint32Value();
        } else {
            rawStream.height = 1296;  // Default sensor resolution
        }

        cameraConfig.rawStream = rawStream;
    }

    // Parse stream configuration
    if (config.Has("streams")) {
        const auto streams = config.Get("streams").As<Napi::Array>();

        for (uint32_t i = 0; i < streams.Length(); ++i) {
            auto streamObj = streams.Get(i).As<Napi::Object>();
            auto typeStr = streamObj.Get("type").As<Napi::String>().Utf8Value();

            lcam::StreamConfig sc;

            if (typeStr == "jpeg") {
                sc.type = lcam::StreamType::JPEG;
            } else if (typeStr == "rgb") {
                sc.type = lcam::StreamType::RGB;
            } else {
                continue;  // Skip unknown types
            }

            if (streamObj.Has("width")) sc.width = streamObj.Get("width").As<Napi::Number>().Uint32Value();
            if (streamObj.Has("height")) sc.height = streamObj.Get("height").As<Napi::Number>().Uint32Value();

            cameraConfig.streams.push_back(sc);
        }
    }

    // Parse initial control values
    if (config.Has("controls")) {
        cameraConfig.initialControls = parseControls(config.Get("controls").As<Napi::Object>());
    }

    // Parse JPEG encoder queue size
    if (config.Has("jpegEncoderQueueSize")) {
        cameraConfig.jpegEncoderQueueSize = config.Get("jpegEncoderQueueSize").As<Napi::Number>().Uint32Value();
    }

    camera_ = std::make_unique<lcam::CameraManager>();
    if (!camera_->initialize(cameraConfig)) {
        Napi::Error::New(env, "Failed to initialize camera").ThrowAsJavaScriptException();
    }
}

NodeCamera::~NodeCamera() {
    if (camera_) camera_->stop();
    if (hasEventHandler_) tsfn_.Release();
}

Napi::Value NodeCamera::On(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (!info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected event name and callback").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto event = info[0].As<Napi::String>().Utf8Value();

    if (event == "frame" || event == "error") {
        if (hasEventHandler_) tsfn_.Release();

        // Create thread-safe function for callbacks
        tsfn_ = Napi::ThreadSafeFunction::New(
            env,
            info[1].As<Napi::Function>(),
            "camera_events",
            4,  // Unlimited queue
            1   // Single thread
        );
        hasEventHandler_ = true;
    }

    return env.Undefined();
}

Napi::Value NodeCamera::Start(const Napi::CallbackInfo &info) {
    const auto env = info.Env();
    if (!hasEventHandler_) {
        Napi::Error::New(env, "Event handler not set").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    struct EventData {
        enum Type { FRAME, ERROR } type;

        lcam::StreamType streamType;
        lcam::Frame frame;
        std::string error;
    };

    const bool success = camera_->start(
        // Frame callback
        [this](lcam::StreamType type, const lcam::Frame &frame) {
            auto *data = new EventData{
                EventData::FRAME,
                type,
                frame,
                ""
            };

            tsfn_.BlockingCall(data, [](Napi::Env env, Napi::Function cb, EventData *data) {
                auto event = Napi::Object::New(env);
                event.Set("type", "frame");
                event.Set("stream", data->streamType == lcam::StreamType::JPEG ? "jpeg" : "rgb");

                // Create zero-copy buffer
                auto buffer = Napi::Buffer<uint8_t>::New(
                    env,
                    const_cast<uint8_t *>(data->frame.data.data()),
                    data->frame.data.size(),
                    [](Napi::Env env, uint8_t *finalizeData, EventData *hint) {
                        delete hint;  // Cleanup when buffer is GC'd
                    },
                    data
                );

                auto frameObj = Napi::Object::New(env);
                frameObj.Set("data", buffer);
                frameObj.Set("timestamp", Napi::BigInt::New(env, data->frame.timestamp));
                frameObj.Set("sequence", data->frame.sequence);

                event.Set("frame", frameObj);
                cb.Call({event});
            });
        },
        // Error callback
        [this](const std::string &error) {
            auto *data = new EventData{
                EventData::ERROR,
                lcam::StreamType::RAW,
                {},
                error
            };

            tsfn_.BlockingCall(data, [](Napi::Env env, Napi::Function cb, EventData *data) {
                auto event = Napi::Object::New(env);
                event.Set("type", "error");
                event.Set("error", data->error);
                cb.Call({event});
                delete data;
            });
        }
    );

    return Napi::Boolean::New(env, success);
}

Napi::Value NodeCamera::Stop(const Napi::CallbackInfo &info) {
    camera_->stop();
    return info.Env().Undefined();
}

Napi::Value NodeCamera::SetControls(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (!info[0].IsObject()) {
        Napi::TypeError::New(env, "Controls object expected").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto controls = parseControls(info[0].As<Napi::Object>());
    return Napi::Boolean::New(env, camera_->setControls(controls));
}

Napi::Value NodeCamera::GetControls(const Napi::CallbackInfo &info) {
    return controlsToObject(info.Env(), camera_->getControls());
}

Napi::Value NodeCamera::GetCapabilities(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    auto caps = camera_->getCapabilities();
    auto result = Napi::Object::New(env);

    // Helper to create range object
    auto createRange = [&env](const auto &range) {
        auto obj = Napi::Object::New(env);
        obj.Set("min", range->min);
        obj.Set("max", range->max);
        obj.Set("default", range->def);
        return obj;
    };

    if (caps.exposureTime) result.Set("exposureTime", createRange(caps.exposureTime));
    if (caps.analogueGain) result.Set("analogueGain", createRange(caps.analogueGain));
    if (caps.lensPosition) result.Set("lensPosition", createRange(caps.lensPosition));

    // Convert string arrays to JS arrays
    auto createArray = [&env](const auto &vec) {
        auto arr = Napi::Array::New(env, vec.size());
        for (size_t i = 0; i < vec.size(); ++i) {
            arr[i] = Napi::String::New(env, vec[i]);
        }
        return arr;
    };

    result.Set("afModes", createArray(caps.afModes));
    result.Set("awbModes", createArray(caps.awbModes));

    return result;
}

lcam::Controls NodeCamera::parseControls(const Napi::Object &obj) {
    lcam::Controls controls;

    // Helper to get optional int value
    auto getInt = [&](const char *key, auto &target) {
        if (obj.Has(key)) target = obj.Get(key).As<Napi::Number>().Int32Value();
    };

    // Helper to get optional float value
    auto getFloat = [&](const char *key, auto &target) {
        if (obj.Has(key)) target = obj.Get(key).As<Napi::Number>().FloatValue();
    };

    getInt("exposureMode", controls.exposureMode);
    getInt("exposureTime", controls.exposureTime);
    getFloat("analogueGain", controls.analogueGain);
    getInt("afMode", controls.afMode);
    getInt("afTrigger", controls.afTrigger);
    getFloat("lensPosition", controls.lensPosition);
    getInt("awbMode", controls.awbMode);

    // Parse colour gains array
    if (obj.Has("colourGains")) {
        auto gains = obj.Get("colourGains").As<Napi::Array>();
        if (gains.Length() >= 2) {
            controls.colourGains = {
                {
                    gains.Get(uint32_t(0)).As<Napi::Number>().FloatValue(),
                    gains.Get(uint32_t(1)).As<Napi::Number>().FloatValue()
                }
            };
        }
    }

    getFloat("brightness", controls.brightness);
    getFloat("contrast", controls.contrast);
    getFloat("saturation", controls.saturation);
    getFloat("sharpness", controls.sharpness);
    getInt("targetFps", controls.targetFps);
    getInt("jpegQuality", controls.jpegQuality);

    return controls;
}

Napi::Object NodeCamera::controlsToObject(Napi::Env env, const lcam::Controls &controls) {
    auto obj = Napi::Object::New(env);

    // Helper to set optional values
    auto setIf = [&](const char *key, const auto &value) {
        if (value) obj.Set(key, *value);
    };

    setIf("exposureMode", controls.exposureMode);
    setIf("exposureTime", controls.exposureTime);
    setIf("analogueGain", controls.analogueGain);
    setIf("afMode", controls.afMode);
    setIf("lensPosition", controls.lensPosition);
    setIf("awbMode", controls.awbMode);

    // Convert colour gains to JS array
    if (controls.colourGains) {
        auto gains = Napi::Array::New(env, 2);
        gains[uint32_t(0)] = (*controls.colourGains)[0];
        gains[uint32_t(1)] = (*controls.colourGains)[1];
        obj.Set("colourGains", gains);
    }

    setIf("brightness", controls.brightness);
    setIf("contrast", controls.contrast);
    setIf("saturation", controls.saturation);
    setIf("sharpness", controls.sharpness);
    setIf("targetFps", controls.targetFps);
    setIf("jpegQuality", controls.jpegQuality);

    return obj;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return NodeCamera::Init(env, exports);
}

NODE_API_MODULE(libcamera_addon, Init)
