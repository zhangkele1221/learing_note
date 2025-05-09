#include <alog/Logger.h>
#include <unordered_set>
#include <json2pb/rapidjson.h>
#include "brpc/options.pb.h"  // for brpc::CompressType
#include "RequestContextImpl.h"
#include "ModuleScheduler.h"
//#include "sw/server/SessionLocalData.h"
#include "Log.h"
#include "brpc/controller.h"
#include "google/protobuf/descriptor.h"
#include "json2pb/pb_to_json.h"
#include "butil/third_party/rapidjson/rapidjson.h"
#include "butil/endpoint.h"

DECLARE_string(cat_domain);

namespace sw {

DECLARE_bool(enable_track_log);

DEFINE_bool(use_tiger_track_log_format, false, "use tiger track log format");

DECLARE_AND_SETUP_LOGGER(sw, RequestContextImpl);

static_assert(brpc::COMPRESS_TYPE_NONE ==
              static_cast<brpc::CompressType>(CompressType::COMPRESS_TYPE_NONE),
              "COMPRESS_TYPE_NONE mismatch");
static_assert(brpc::COMPRESS_TYPE_SNAPPY ==
              static_cast<brpc::CompressType>(CompressType::COMPRESS_TYPE_SNAPPY),
              "COMPRESS_TYPE_SNAPPY mismatch");
static_assert(brpc::COMPRESS_TYPE_GZIP ==
              static_cast<brpc::CompressType>(CompressType::COMPRESS_TYPE_GZIP),
              "COMPRESS_TYPE_GZIP mismatch");
static_assert(brpc::COMPRESS_TYPE_ZLIB ==
              static_cast<brpc::CompressType>(CompressType::COMPRESS_TYPE_ZLIB),
              "COMPRESS_TYPE_ZLIB mismatch");

RequestContextImpl::RequestContextImpl() :
    _session_local_data(nullptr),
    _container(nullptr),
    _p_scheduler(nullptr),
    _p_request(nullptr),
    _p_response(nullptr),
    _controller(nullptr),
    _begin_time_us(0),
    _track_written(false)
{}

RunnableModule* RequestContextImpl::get_runnable_module(const char* name) {
    return _p_scheduler->get_runnable_module(name);
}

//ReloadableModule* RequestContextImpl::get_reloadable_module(const char* name) {
//    return _p_scheduler->get_reloadable_module(name);
//}

bool RequestContextImpl::construct(SessionLocalData *container) {
    _container = container;
    _p_scheduler = container->scheduler;
    _session_local_data = container->app_data;

    return true;
}

bool RequestContextImpl::init(brpc::Controller*          controller,
                              const ::google::protobuf::Message*          request,
                              ::google::protobuf::Message*                response,
                              ::google::protobuf::Closure*                done) {
    _begin_time_us = butil::gettimeofday_us();
    _controller = controller;
    _p_request = request;
    _p_response = response;
    _service_full_name = controller->method()->service()->full_name();
    _method_name = controller->method()->name();
    if (_session_local_data && !_session_local_data->init(this)) {
        return false;
    }
    return _p_scheduler->init(this, done);
}

void RequestContextImpl::reset() {
    /*
    _p_scheduler->reset();
    _p_request = nullptr;
    _p_response = nullptr;
    _controller = nullptr;
    _begin_time_us = 0;
    _key_value_pairs.clear();
    _service_full_name.clear();
    _method_name.clear();
    _track_written = false;
    if (_session_local_data) {
        _session_local_data->reset();
    }
    */
}

int64_t RequestContextImpl::latency_since_first_byte_us() const {
    return _controller && _controller->latency_us() != 0
           ? _controller->latency_us() : (butil::gettimeofday_us() - _begin_time_us);
}

bool RequestContextImpl::submit_runnable(const RunnableElement & element) {
    return _p_scheduler->submit(element);
}

void RequestContextImpl::set_response_compress_type(CompressType compress_type) {
    _controller->set_response_compress_type(static_cast<brpc::CompressType>(compress_type));
}

bool RequestContextImpl::track(const ::google::protobuf::Message& message) {
    std::string topic;
    static const std::string tag = "default";
    return track(message, topic, tag);
}

bool RequestContextImpl::track(const ::google::protobuf::Message & message, const std::string& topic,
                                const std::string& tag) {
    if (!FLAGS_enable_track_log || _track_written) {
        return false;
    }
    static const std::unordered_set<std::string> reserved_keys = {
            "red_log_id", "red_log_tag", "red_app_name", "red_log_time", "red_log_host",
    };

    const auto * reflection = message.GetReflection();
    const auto * descriptor = message.GetDescriptor();
    int ext_range_count = descriptor->extension_range_count();
    int field_count = descriptor->field_count();
    for (int i = 0; i < ext_range_count; ++i) {
        const auto* ext_range = descriptor->extension_range(i);
        for (int tag_number = ext_range->start; tag_number < ext_range->end; ++tag_number) {
            const auto * field = reflection->FindKnownExtensionByNumber(tag_number);
            if (field && reserved_keys.find(field->name()) != reserved_keys.end()) {
                TERR("reserved key[%s] found in message", field->name().c_str());
                return false;
            }
        }
    }

    for (int i = 0; i < field_count; ++i) {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);
        if (field && reserved_keys.find(field->name()) != reserved_keys.end()) {
            TERR("reserved key[%s] found in message", field->name().c_str());
            return false;
        }
    }

    std::string error;
    json2pb::Pb2JsonOptions options;
    std::string json;
    // TODO(ziqian): make this configurable through gflags, reserve memory more wisely
    json.reserve(4096);
    if (json2pb::ProtoMessageToJson(message, &json, options, &error)) {
        if (json.size() < 2 || json.back() != '}') {
            TERR("unexpected json got, text format is: %s", message.ShortDebugString().c_str());
            return false;
        }

        json.pop_back();
    } else {
        TERR("convert track message to json failed, details: %s", error.c_str());
        return false;
    }

    std::string output;
    if (FLAGS_use_tiger_track_log_format) {
        if (topic.empty()) {
            TERR("topic should not be empty when use tiger_log_format");
            return false;
        }
        if (butil::string_appendf(&output, "%s %s %s", topic.c_str(), tag.c_str(), json.c_str()) != 0) {
            TERR("build track meta failed");
            return false;
        }
    } else {
        output = std::move(json);
    }

    int rc = butil::string_appendf(&output,
            ",\"red_log_time\":\"%ld\""
            ",\"red_log_tag\":\"%s\""
            ",\"red_log_id\":\"%016" PRIx64 "\""
            ",\"red_log_host\":\"%s\""
            ",\"red_app_name\":\"%s\"}\n",
                                   butil::gettimeofday_us() / 1000,
                                   tag.c_str(),
                                   alog::Logger::getLogId(),
                                   butil::my_hostname(),
                                   FLAGS_cat_domain.c_str());

    if (rc != 0) {
        TERR("build track meta failed");
        return false;
    }

    alog::Logger::getLogger("_sw_tracker")->logPureMessage(alog::LOG_LEVEL_INFO, std::move(output));

    _track_written = true;
    return true;
}

}  // namespace sw

