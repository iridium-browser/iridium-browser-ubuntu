//* Copyright 2017 The Dawn Authors
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at
//*
//*     http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.

#include "dawn_wire/TypeTraits_autogen.h"
#include "dawn_wire/Wire.h"
#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/WireDeserializeAllocator.h"

#include "common/Assert.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

namespace dawn_wire {

    namespace server {
        class Server;

        struct MapUserdata {
            Server* server;
            ObjectHandle buffer;
            uint32_t requestSerial;
            uint32_t size;
            bool isWrite;
        };

        struct FenceCompletionUserdata {
            Server* server;
            ObjectHandle fence;
            uint64_t value;
        };

        template <typename T>
        struct ObjectDataBase {
            //* The backend-provided handle and serial to this object.
            T handle;
            uint32_t serial = 0;

            //* Used by the error-propagation mechanism to know if this object is an error.
            //* TODO(cwallez@chromium.org): this is doubling the memory usage of
            //* std::vector<ObjectDataBase> consider making it a special marker value in handle instead.
            bool valid;
            //* Whether this object has been allocated, used by the KnownObjects queries
            //* TODO(cwallez@chromium.org): make this an internal bit vector in KnownObjects.
            bool allocated;
        };

        //* Stores what the backend knows about the type.
        template<typename T, bool IsBuilder = IsBuilderType<T>::value>
        struct ObjectData : public ObjectDataBase<T> {
        };


        template <typename T>
        struct ObjectData<T, true> : public ObjectDataBase<T> {
            ObjectHandle builtObject = ObjectHandle{0, 0};
        };

        template <>
        struct ObjectData<dawnBuffer, false> : public ObjectDataBase<dawnBuffer> {
            void* mappedData = nullptr;
            size_t mappedDataSize = 0;
        };

        //* Keeps track of the mapping between client IDs and backend objects.
        template<typename T>
        class KnownObjects {
            public:
                using Data = ObjectData<T>;

                KnownObjects() {
                    //* Pre-allocate ID 0 to refer to the null handle.
                    Data nullObject;
                    nullObject.handle = nullptr;
                    nullObject.valid = true;
                    nullObject.allocated = true;
                    mKnown.push_back(nullObject);
                }

                //* Get a backend objects for a given client ID.
                //* Returns nullptr if the ID hasn't previously been allocated.
                const Data* Get(uint32_t id) const {
                    if (id >= mKnown.size()) {
                        return nullptr;
                    }

                    const Data* data = &mKnown[id];

                    if (!data->allocated) {
                        return nullptr;
                    }

                    return data;
                }
                Data* Get(uint32_t id) {
                    if (id >= mKnown.size()) {
                        return nullptr;
                    }

                    Data* data = &mKnown[id];

                    if (!data->allocated) {
                        return nullptr;
                    }

                    return data;
                }

                //* Allocates the data for a given ID and returns it.
                //* Returns nullptr if the ID is already allocated, or too far ahead.
                //* Invalidates all the Data*
                Data* Allocate(uint32_t id) {
                    if (id > mKnown.size()) {
                        return nullptr;
                    }

                    Data data;
                    data.allocated = true;
                    data.valid = false;
                    data.handle = nullptr;

                    if (id >= mKnown.size()) {
                        mKnown.push_back(data);
                        return &mKnown.back();
                    }

                    if (mKnown[id].allocated) {
                        return nullptr;
                    }

                    mKnown[id] = data;
                    return &mKnown[id];
                }

                //* Marks an ID as deallocated
                void Free(uint32_t id) {
                    ASSERT(id < mKnown.size());
                    mKnown[id].allocated = false;
                }

                std::vector<T> AcquireAllHandles() {
                    std::vector<T> objects;
                    for (Data& data : mKnown) {
                        if (data.allocated && data.handle != nullptr) {
                            objects.push_back(data.handle);
                            data.valid = false;
                            data.allocated = false;
                            data.handle = nullptr;
                        }
                    }

                    return objects;
                }

            private:
                std::vector<Data> mKnown;
        };

        // ObjectIds are lost in deserialization. Store the ids of deserialized
        // objects here so they can be used in command handlers. This is useful
        // for creating ReturnWireCmds which contain client ids
        template <typename T>
        class ObjectIdLookupTable {
          public:
            void Store(T key, ObjectId id) {
                mTable[key] = id;
            }

            // Return the cached ObjectId, or 0 (null handle)
            ObjectId Get(T key) const {
                const auto it = mTable.find(key);
                if (it != mTable.end()) {
                    return it->second;
                }
                return 0;
            }

            void Remove(T key) {
                auto it = mTable.find(key);
                if (it != mTable.end()) {
                    mTable.erase(it);
                }
            }

          private:
            std::map<T, ObjectId> mTable;
        };

        void ForwardDeviceErrorToServer(const char* message, dawnCallbackUserdata userdata);

        {% for type in by_category["object"] if type.is_builder%}
            void Forward{{type.name.CamelCase()}}ToClient(dawnBuilderErrorStatus status, const char* message, dawnCallbackUserdata userdata1, dawnCallbackUserdata userdata2);
        {% endfor %}

        void ForwardBufferMapReadAsync(dawnBufferMapAsyncStatus status, const void* ptr, dawnCallbackUserdata userdata);
        void ForwardBufferMapWriteAsync(dawnBufferMapAsyncStatus status, void* ptr, dawnCallbackUserdata userdata);
        void ForwardFenceCompletedValue(dawnFenceCompletionStatus status,
                                        dawnCallbackUserdata userdata);

        class Server : public CommandHandler, public ObjectIdResolver {
            public:
                Server(dawnDevice device, const dawnProcTable& procs, CommandSerializer* serializer)
                    : mProcs(procs), mSerializer(serializer) {
                    //* The client-server knowledge is bootstrapped with device 1.
                    auto* deviceData = mKnownDevice.Allocate(1);
                    deviceData->handle = device;
                    deviceData->valid = true;

                    auto userdata = static_cast<dawnCallbackUserdata>(reinterpret_cast<intptr_t>(this));
                    procs.deviceSetErrorCallback(device, ForwardDeviceErrorToServer, userdata);
                }

                ~Server() override {
                    //* Free all objects when the server is destroyed
                    {% for type in by_category["object"] if type.name.canonical_case() != "device" %}
                        {
                            std::vector<{{as_cType(type.name)}}> handles = mKnown{{type.name.CamelCase()}}.AcquireAllHandles();
                            for ({{as_cType(type.name)}} handle : handles) {
                                mProcs.{{as_varName(type.name, Name("release"))}}(handle);
                            }
                        }
                    {% endfor %}
                }

                void OnDeviceError(const char* message) {
                    ReturnDeviceErrorCallbackCmd cmd;
                    cmd.message = message;

                    size_t requiredSize = cmd.GetRequiredSize();
                    char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
                    cmd.Serialize(allocatedBuffer);
                }

                {% for type in by_category["object"] if type.is_builder%}
                    {% set Type = type.name.CamelCase() %}
                    void On{{Type}}Error(dawnBuilderErrorStatus status, const char* message, uint32_t id, uint32_t serial) {
                        auto* builder = mKnown{{Type}}.Get(id);

                        if (builder == nullptr || builder->serial != serial) {
                            return;
                        }

                        if (status != DAWN_BUILDER_ERROR_STATUS_SUCCESS) {
                            builder->valid = false;
                        }

                        if (status != DAWN_BUILDER_ERROR_STATUS_UNKNOWN) {
                            //* Unknown is the only status that can be returned without a call to GetResult
                            //* so we are guaranteed to have created an object.
                            ASSERT(builder->builtObject.id != 0);

                            Return{{Type}}ErrorCallbackCmd cmd;
                            cmd.builtObject = builder->builtObject;
                            cmd.status = status;
                            cmd.message = message;

                            size_t requiredSize = cmd.GetRequiredSize();
                            char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
                            cmd.Serialize(allocatedBuffer);
                        }
                    }
                {% endfor %}

                void OnMapReadAsyncCallback(dawnBufferMapAsyncStatus status, const void* ptr, MapUserdata* userdata) {
                    std::unique_ptr<MapUserdata> data(userdata);

                    // Skip sending the callback if the buffer has already been destroyed.
                    auto* bufferData = mKnownBuffer.Get(data->buffer.id);
                    if (bufferData == nullptr || bufferData->serial != data->buffer.serial) {
                        return;
                    }

                    ReturnBufferMapReadAsyncCallbackCmd cmd;
                    cmd.buffer = data->buffer;
                    cmd.requestSerial = data->requestSerial;
                    cmd.status = status;
                    cmd.dataLength = 0;
                    cmd.data = reinterpret_cast<const uint8_t*>(ptr);

                    if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS) {
                        cmd.dataLength = data->size;
                    }

                    size_t requiredSize = cmd.GetRequiredSize();
                    char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
                    cmd.Serialize(allocatedBuffer);
                }

                void OnMapWriteAsyncCallback(dawnBufferMapAsyncStatus status, void* ptr, MapUserdata* userdata) {
                    std::unique_ptr<MapUserdata> data(userdata);

                    // Skip sending the callback if the buffer has already been destroyed.
                    auto* bufferData = mKnownBuffer.Get(data->buffer.id);
                    if (bufferData == nullptr || bufferData->serial != data->buffer.serial) {
                        return;
                    }

                    ReturnBufferMapWriteAsyncCallbackCmd cmd;
                    cmd.buffer = data->buffer;
                    cmd.requestSerial = data->requestSerial;
                    cmd.status = status;

                    size_t requiredSize = cmd.GetRequiredSize();
                    char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
                    cmd.Serialize(allocatedBuffer);

                    if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS) {
                        bufferData->mappedData = ptr;
                        bufferData->mappedDataSize = data->size;
                    }
                }

                void OnFenceCompletedValueUpdated(FenceCompletionUserdata* userdata) {
                    std::unique_ptr<FenceCompletionUserdata> data(userdata);

                    ReturnFenceUpdateCompletedValueCmd cmd;
                    cmd.fence = data->fence;
                    cmd.value = data->value;

                    size_t requiredSize = cmd.GetRequiredSize();
                    char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
                    cmd.Serialize(allocatedBuffer);
                }

                const char* HandleCommands(const char* commands, size_t size) override {
                    mProcs.deviceTick(mKnownDevice.Get(1)->handle);

                    while (size >= sizeof(WireCmd)) {
                        WireCmd cmdId = *reinterpret_cast<const WireCmd*>(commands);

                        bool success = false;
                        switch (cmdId) {
                            {% for command in cmd_records["command"] %}
                                case WireCmd::{{command.name.CamelCase()}}:
                                    success = Handle{{command.name.CamelCase()}}(&commands, &size);
                                    break;
                            {% endfor %}
                            default:
                                success = false;
                        }

                        if (!success) {
                            return nullptr;
                        }
                        mAllocator.Reset();
                    }

                    if (size != 0) {
                        return nullptr;
                    }

                    return commands;
                }

            private:
                dawnProcTable mProcs;
                CommandSerializer* mSerializer = nullptr;

                WireDeserializeAllocator mAllocator;

                void* GetCmdSpace(size_t size) {
                    return mSerializer->GetCmdSpace(size);
                }

                // Implementation of the ObjectIdResolver interface
                {% for type in by_category["object"] %}
                    DeserializeResult GetFromId(ObjectId id, {{as_cType(type.name)}}* out) const final {
                        auto data = mKnown{{type.name.CamelCase()}}.Get(id);
                        if (data == nullptr) {
                            return DeserializeResult::FatalError;
                        }

                        *out = data->handle;
                        if (data->valid) {
                            return DeserializeResult::Success;
                        } else {
                            return DeserializeResult::ErrorObject;
                        }
                    }

                    DeserializeResult GetOptionalFromId(ObjectId id, {{as_cType(type.name)}}* out) const final {
                        if (id == 0) {
                            *out = nullptr;
                            return DeserializeResult::Success;
                        }

                        return GetFromId(id, out);
                    }
                {% endfor %}

                //* The list of known IDs for each object type.
                {% for type in by_category["object"] %}
                    KnownObjects<{{as_cType(type.name)}}> mKnown{{type.name.CamelCase()}};
                {% endfor %}

                {% for type in by_category["object"] if type.name.CamelCase() in server_reverse_lookup_objects %}
                    ObjectIdLookupTable<{{as_cType(type.name)}}> m{{type.name.CamelCase()}}IdTable;
                {% endfor %}

                bool PreHandleBufferUnmap(const BufferUnmapCmd& cmd) {
                    auto* selfData = mKnownBuffer.Get(cmd.selfId);
                    ASSERT(selfData != nullptr);

                    selfData->mappedData = nullptr;

                    return true;
                }

                bool PostHandleQueueSignal(const QueueSignalCmd& cmd) {
                    if (cmd.fence == nullptr) {
                        return false;
                    }
                    ObjectId fenceId = mFenceIdTable.Get(cmd.fence);
                    ASSERT(fenceId != 0);
                    auto* fence = mKnownFence.Get(fenceId);
                    ASSERT(fence != nullptr);

                    auto* data = new FenceCompletionUserdata;
                    data->server = this;
                    data->fence = ObjectHandle{fenceId, fence->serial};
                    data->value = cmd.signalValue;

                    auto userdata = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(data));
                    mProcs.fenceOnCompletion(cmd.fence, cmd.signalValue, ForwardFenceCompletedValue, userdata);
                    return true;
                }

                //* Implementation of the command handlers
                {% for type in by_category["object"] %}
                    {% for method in type.methods %}
                        {% set Suffix = as_MethodSuffix(type.name, method.name) %}
                        {% if Suffix not in client_side_commands %}
                            //* The generic command handlers

                            bool Handle{{Suffix}}(const char** commands, size_t* size) {
                                {{Suffix}}Cmd cmd;
                                DeserializeResult deserializeResult = cmd.Deserialize(commands, size, &mAllocator, *this);

                                if (deserializeResult == DeserializeResult::FatalError) {
                                    return false;
                                }

                                {% if Suffix in server_custom_pre_handler_commands %}
                                    if (!PreHandle{{Suffix}}(cmd)) {
                                        return false;
                                    }
                                {% endif %}

                                //* Unpack 'self'
                                auto* selfData = mKnown{{type.name.CamelCase()}}.Get(cmd.selfId);
                                ASSERT(selfData != nullptr);

                                //* In all cases allocate the object data as it will be refered-to by the client.
                                {% set return_type = method.return_type %}
                                {% set returns = return_type.name.canonical_case() != "void" %}
                                {% if returns %}
                                    {% set Type = method.return_type.name.CamelCase() %}
                                    auto* resultData = mKnown{{Type}}.Allocate(cmd.result.id);
                                    if (resultData == nullptr) {
                                        return false;
                                    }
                                    resultData->serial = cmd.result.serial;

                                    {% if type.is_builder %}
                                        selfData->builtObject = cmd.result;
                                    {% endif %}
                                {% endif %}

                                //* After the data is allocated, apply the argument error propagation mechanism
                                if (deserializeResult == DeserializeResult::ErrorObject) {
                                    {% if type.is_builder %}
                                        selfData->valid = false;
                                        //* If we are in GetResult, fake an error callback
                                        {% if returns %}
                                            On{{type.name.CamelCase()}}Error(DAWN_BUILDER_ERROR_STATUS_ERROR, "Maybe monad", cmd.selfId, selfData->serial);
                                        {% endif %}
                                    {% endif %}
                                    return true;
                                }

                                {% if returns %}
                                    auto result ={{" "}}
                                {%- endif %}
                                mProcs.{{as_varName(type.name, method.name)}}(cmd.self
                                    {%- for arg in method.arguments -%}
                                        , cmd.{{as_varName(arg.name)}}
                                    {%- endfor -%}
                                );

                                {% if Suffix in server_custom_post_handler_commands %}
                                    if (!PostHandle{{Suffix}}(cmd)) {
                                        return false;
                                    }
                                {% endif %}

                                {% if returns %}
                                    resultData->handle = result;
                                    resultData->valid = result != nullptr;

                                    {% if return_type.name.CamelCase() in server_reverse_lookup_objects %}
                                        //* For created objects, store a mapping from them back to their client IDs
                                        if (result) {
                                            m{{return_type.name.CamelCase()}}IdTable.Store(result, cmd.result.id);
                                        }
                                    {% endif %}

                                    //* builders remember the ID of the object they built so that they can send it
                                    //* in the callback to the client.
                                    {% if return_type.is_builder %}
                                        if (result != nullptr) {
                                            uint64_t userdata1 = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
                                            uint64_t userdata2 = (uint64_t(resultData->serial) << uint64_t(32)) + cmd.result.id;
                                            mProcs.{{as_varName(return_type.name, Name("set error callback"))}}(result, Forward{{return_type.name.CamelCase()}}ToClient, userdata1, userdata2);
                                        }
                                    {% endif %}
                                {% endif %}

                                return true;
                            }
                        {% endif %}
                    {% endfor %}
                {% endfor %}

                bool HandleBufferMapAsync(const char** commands, size_t* size) {
                    //* These requests are just forwarded to the buffer, with userdata containing what the client
                    //* will require in the return command.
                    BufferMapAsyncCmd cmd;
                    DeserializeResult deserializeResult = cmd.Deserialize(commands, size, &mAllocator);

                    if (deserializeResult == DeserializeResult::FatalError) {
                        return false;
                    }

                    ObjectId bufferId = cmd.bufferId;
                    uint32_t requestSerial = cmd.requestSerial;
                    uint32_t requestSize = cmd.size;
                    uint32_t requestStart = cmd.start;
                    bool isWrite = cmd.isWrite;

                    //* The null object isn't valid as `self`
                    if (bufferId == 0) {
                        return false;
                    }

                    auto* buffer = mKnownBuffer.Get(bufferId);
                    if (buffer == nullptr) {
                        return false;
                    }

                    auto* data = new MapUserdata;
                    data->server = this;
                    data->buffer = ObjectHandle{bufferId, buffer->serial};
                    data->requestSerial = requestSerial;
                    data->size = requestSize;
                    data->isWrite = isWrite;

                    auto userdata = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(data));

                    if (!buffer->valid) {
                        //* Fake the buffer returning a failure, data will be freed in this call.
                        if (isWrite) {
                            ForwardBufferMapWriteAsync(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, userdata);
                        } else {
                            ForwardBufferMapReadAsync(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, userdata);
                        }
                        return true;
                    }

                    if (isWrite) {
                        mProcs.bufferMapWriteAsync(buffer->handle, requestStart, requestSize, ForwardBufferMapWriteAsync, userdata);
                    } else {
                        mProcs.bufferMapReadAsync(buffer->handle, requestStart, requestSize, ForwardBufferMapReadAsync, userdata);
                    }

                    return true;
                }

                bool HandleBufferUpdateMappedData(const char** commands, size_t* size) {
                    BufferUpdateMappedDataCmd cmd;
                    DeserializeResult deserializeResult = cmd.Deserialize(commands, size, &mAllocator);

                    if (deserializeResult == DeserializeResult::FatalError) {
                        return false;
                    }

                    ObjectId bufferId = cmd.bufferId;
                    size_t dataLength = cmd.dataLength;

                    //* The null object isn't valid as `self`
                    if (bufferId == 0) {
                        return false;
                    }

                    auto* buffer = mKnownBuffer.Get(bufferId);
                    if (buffer == nullptr || !buffer->valid || buffer->mappedData == nullptr ||
                        buffer->mappedDataSize != dataLength) {
                        return false;
                    }

                    DAWN_ASSERT(cmd.data != nullptr);

                    memcpy(buffer->mappedData, cmd.data, dataLength);

                    return true;
                }

                bool HandleDestroyObject(const char** commands, size_t* size) {
                    DestroyObjectCmd cmd;
                    DeserializeResult deserializeResult = cmd.Deserialize(commands, size, &mAllocator);

                    if (deserializeResult == DeserializeResult::FatalError) {
                        return false;
                    }

                    ObjectId objectId = cmd.objectId;
                    //* ID 0 are reserved for nullptr and cannot be destroyed.
                    if (objectId == 0) {
                        return false;
                    }

                    switch (cmd.objectType) {
                        {% for type in by_category["object"] %}
                            {% set ObjectType = type.name.CamelCase() %}
                            case ObjectType::{{ObjectType}}: {
                                {% if ObjectType == "Device" %}
                                    //* Freeing the device has to be done out of band.
                                    return false;
                                {% else %}
                                    auto* data = mKnown{{type.name.CamelCase()}}.Get(objectId);
                                    if (data == nullptr) {
                                        return false;
                                    }
                                    {% if type.name.CamelCase() in server_reverse_lookup_objects %}
                                        m{{type.name.CamelCase()}}IdTable.Remove(data->handle);
                                    {% endif %}

                                    if (data->handle != nullptr) {
                                        mProcs.{{as_varName(type.name, Name("release"))}}(data->handle);
                                    }

                                    mKnown{{type.name.CamelCase()}}.Free(objectId);
                                    return true;
                                {% endif %}
                            }
                        {% endfor %}
                        default:
                            UNREACHABLE();
                    }
                }
        };

        void ForwardDeviceErrorToServer(const char* message, dawnCallbackUserdata userdata) {
            auto server = reinterpret_cast<Server*>(static_cast<intptr_t>(userdata));
            server->OnDeviceError(message);
        }

        {% for type in by_category["object"] if type.is_builder%}
            void Forward{{type.name.CamelCase()}}ToClient(dawnBuilderErrorStatus status, const char* message, dawnCallbackUserdata userdata1, dawnCallbackUserdata userdata2) {
                auto server = reinterpret_cast<Server*>(static_cast<uintptr_t>(userdata1));
                uint32_t id = userdata2 & 0xFFFFFFFFu;
                uint32_t serial = userdata2 >> uint64_t(32);
                server->On{{type.name.CamelCase()}}Error(status, message, id, serial);
            }
        {% endfor %}

        void ForwardBufferMapReadAsync(dawnBufferMapAsyncStatus status, const void* ptr, dawnCallbackUserdata userdata) {
            auto data = reinterpret_cast<MapUserdata*>(static_cast<uintptr_t>(userdata));
            data->server->OnMapReadAsyncCallback(status, ptr, data);
        }

        void ForwardBufferMapWriteAsync(dawnBufferMapAsyncStatus status, void* ptr, dawnCallbackUserdata userdata) {
            auto data = reinterpret_cast<MapUserdata*>(static_cast<uintptr_t>(userdata));
            data->server->OnMapWriteAsyncCallback(status, ptr, data);
        }

        void ForwardFenceCompletedValue(dawnFenceCompletionStatus status, dawnCallbackUserdata userdata) {
            auto data = reinterpret_cast<FenceCompletionUserdata*>(static_cast<uintptr_t>(userdata));
            if (status == DAWN_FENCE_COMPLETION_STATUS_SUCCESS) {
                data->server->OnFenceCompletedValueUpdated(data);
            }
        }
    }

    CommandHandler* NewServerCommandHandler(dawnDevice device, const dawnProcTable& procs, CommandSerializer* serializer) {
        return new server::Server(device, procs, serializer);
    }

}  // namespace dawn_wire
