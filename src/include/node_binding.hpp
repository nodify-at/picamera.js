#pragma once

#include <napi.h>
#include "camera_manager.hpp"

/**
 * Node.js addon wrapper for camera functionality
 */
class NodeCamera : public Napi::ObjectWrap<NodeCamera> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    NodeCamera(const Napi::CallbackInfo& info);
    ~NodeCamera();

private:
    static Napi::FunctionReference constructor;

    // JavaScript method bindings
    Napi::Value Start(const Napi::CallbackInfo& info);
    Napi::Value Stop(const Napi::CallbackInfo& info);
    Napi::Value SetControls(const Napi::CallbackInfo& info);
    Napi::Value GetControls(const Napi::CallbackInfo& info);
    Napi::Value GetCapabilities(const Napi::CallbackInfo& info);
    Napi::Value On(const Napi::CallbackInfo& info);

    // Helper methods for control conversion
    lcam::Controls parseControls(const Napi::Object& obj);
    Napi::Object controlsToObject(Napi::Env env, const lcam::Controls& controls);

    std::unique_ptr<lcam::CameraManager> camera_;
    Napi::ThreadSafeFunction tsfn_;  // Thread-safe callback
    bool hasEventHandler_ = false;
};
