#include "nodeobs_autoconfig.hpp"
#include "shared.hpp"

using namespace std::placeholders;

AutoConfig::~AutoConfig() {
	stop_worker();
	stop_async_runner();
}

void AutoConfig::start_async_runner() {
	if (m_async_callback)
		return;
	std::unique_lock<std::mutex> ul(m_worker_lock);
	// Start v8/uv asynchronous runner.
	m_async_callback = new AutoConfigCallback();
	m_async_callback->set_handler(std::bind(&AutoConfig::callback_handler, this, _1, _2), nullptr);
}

void AutoConfig::stop_async_runner() {
	if (!m_async_callback)
		return;
	std::unique_lock<std::mutex> ul(m_worker_lock);
	// Stop v8/uv asynchronous runner.
	m_async_callback->clear();
	m_async_callback->finalize();
	m_async_callback = nullptr;
}

void AutoConfig::callback_handler(void* data, std::shared_ptr<AutoConfigInfo> item) {
	v8::Isolate *isolate = v8::Isolate::GetCurrent();
	v8::Local<v8::Value> args[1];

	v8::Local<v8::Value> argv = v8::Object::New(isolate);
	argv->ToObject()->Set(v8::String::NewFromUtf8(isolate, "event"),
		v8::String::NewFromUtf8(isolate, item->event.c_str()));
	argv->ToObject()->Set(v8::String::NewFromUtf8(isolate,
		"description"), v8::String::NewFromUtf8(isolate, item->description.c_str()));

	if (item->event.compare("error") != 0) {
		argv->ToObject()->Set(v8::String::NewFromUtf8(isolate,
			"percentage"), v8::Number::New(isolate, item->percentage));
	}

	args[0] = argv;

	Nan::Call(m_callback_function, 1, args);
}

void AutoConfig::start_worker() {
	if (!m_worker_stop)
		return;
	// Launch worker thread.
	m_worker_stop = false;
	m_worker = std::thread(std::bind(&AutoConfig::worker, this));
}

void AutoConfig::stop_worker() {
	if (m_worker_stop != false)
		return;
	// Stop worker thread.
	m_worker_stop = true;
	if (m_worker.joinable()) {
		m_worker.join();
	}
}

void AutoConfig::set_keepalive(v8::Local<v8::Object> obj) {
	if (!m_async_callback)
		return;
	m_async_callback->set_keepalive(obj);
}

void AutoConfig::worker() {
	size_t totalSleepMS = 0;

	while (!m_worker_stop) {
		auto tp_start = std::chrono::high_resolution_clock::now();

		// Validate Connection
		auto conn = Controller::GetInstance().GetConnection();
		if (!conn) {
			goto do_sleep;
		}

		// Call
		{
			std::vector<ipc::value> response =
				conn->call_synchronous_helper("AutoConfig", "Query", {});
			if (!response.size() || (response.size() == 1)) {
				goto do_sleep;
			}

			ErrorCode error = (ErrorCode)response[0].value_union.ui64;
			if (error == ErrorCode::Ok) {
				std::shared_ptr<AutoConfigInfo> data = std::make_shared<AutoConfigInfo>();

				data->event = response[1].value_str;
				data->description = response[2].value_str;
				data->percentage = response[3].value_union.fp64;
				data->param = this;

				m_async_callback->queue(std::move(data));
			}
		}

	do_sleep:
		auto tp_end = std::chrono::high_resolution_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(tp_end - tp_start);
		totalSleepMS = sleepIntervalMS - dur.count();
		std::this_thread::sleep_for(std::chrono::milliseconds(totalSleepMS));
	}
	return;
}

void autoConfig::GetListServer(const v8::FunctionCallbackInfo<v8::Value>& args) {
	std::string service, continent;

	ASSERT_GET_VALUE(args[0], service);
	ASSERT_GET_VALUE(args[1], continent);

	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig",
			"GetListServer", { service, continent });

	ValidateResponse(response);

	v8::Isolate *isolate = v8::Isolate::GetCurrent();
	v8::Local<v8::Array> listServer = v8::Array::New(isolate);

	for (int i = 1; i < response.size(); i++) {
		v8::Local<v8::Object> object = v8::Object::New(isolate);

		object->Set(v8::String::NewFromUtf8(isolate, "server_name"),
			v8::String::NewFromUtf8(isolate, response[i].value_str.c_str()));

		object->Set(v8::String::NewFromUtf8(isolate, "server"),
			v8::String::NewFromUtf8(isolate, response[i].value_str.c_str()));

		listServer->Set(i, object);
	}

	args.GetReturnValue().Set(listServer);
}

static v8::Persistent<v8::Object> autoConfigCallbackObject;

void autoConfig::InitializeAutoConfig(const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Local<v8::Function> callback;
	ASSERT_GET_VALUE(args[0], callback);

	v8::Isolate *isolate = v8::Isolate::GetCurrent();

	v8::Local<v8::Object> serverInfo = args[1].As<v8::Object>();

	v8::String::Utf8Value param0(serverInfo->Get(v8::String::NewFromUtf8(isolate, "continent")));
	std::string continent = std::string(*param0);

	v8::String::Utf8Value param1(serverInfo->Get(v8::String::NewFromUtf8(isolate, "service_name")));
	std::string service = std::string(*param1);

	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig",
			"InitializeAutoConfig", { continent, service });

	ValidateResponse(response);

	// Callback
	autoConfigObject = new AutoConfig();
	autoConfigObject->m_callback_function.Reset(callback);
	autoConfigObject->start_async_runner();
	autoConfigObject->set_keepalive(args.This());
	autoConfigObject->start_worker();
	args.GetReturnValue().Set(true);
}

void autoConfig::StartBandwidthTest(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig", "StartBandwidthTest", {});

	ValidateResponse(response);
}

void autoConfig::StartStreamEncoderTest(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig", "StartStreamEncoderTest", {});

	ValidateResponse(response);
}

void autoConfig::StartRecordingEncoderTest(const v8::FunctionCallbackInfo<v8::Value>& args) {

	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig", "StartRecordingEncoderTest", {});

	ValidateResponse(response);
}

void autoConfig::StartCheckSettings(const v8::FunctionCallbackInfo<v8::Value>& args) {
	std::shared_ptr<AutoConfigInfo> startData = std::make_shared<AutoConfigInfo>();
	startData->event = "starting_step";
	startData->description = "checking_settings";
	startData->percentage = 0;
	startData->param = autoConfigObject;
	autoConfigObject->m_async_callback->queue(std::move(startData));

	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig", "StartCheckSettings", {});

	ValidateResponse(response);

	bool success = (bool)response[1].value_union.ui32;
	std::shared_ptr<AutoConfigInfo> stopData = std::make_shared<AutoConfigInfo>();
	if (!success) {
		stopData->event = "error";
		stopData->description = "invalid_settings";
	} else {
		stopData->event = "stopping_step";
		stopData->description = "checking_settings";
	}

	stopData->percentage = 100;
	stopData->param = autoConfigObject;
	autoConfigObject->m_async_callback->queue(std::move(stopData));
}

void autoConfig::StartSetDefaultSettings(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig", "StartSetDefaultSettings", {});

	ValidateResponse(response);
}

void autoConfig::StartSaveStreamSettings(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig", "StartSaveStreamSettings", {});

	ValidateResponse(response);
}

void autoConfig::StartSaveSettings(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig", "StartSaveSettings", {});

	ValidateResponse(response);
}

void autoConfig::TerminateAutoConfig(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto conn = GetConnection();
	if (!conn) return;

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AutoConfig",
			"TerminateAutoConfig", {});

	ValidateResponse(response);

	autoConfigObject->stop_worker();
	autoConfigObject->stop_async_runner();
	delete autoConfigObject;
}

INITIALIZER(nodeobs_autoconfig) {
	initializerFunctions.push([](v8::Local<v8::Object> exports) {
		NODE_SET_METHOD(exports, "GetListServer", autoConfig::GetListServer);
		NODE_SET_METHOD(exports, "InitializeAutoConfig", autoConfig::InitializeAutoConfig);
		NODE_SET_METHOD(exports, "StartBandwidthTest", autoConfig::StartBandwidthTest);
		NODE_SET_METHOD(exports, "StartStreamEncoderTest", autoConfig::StartStreamEncoderTest);
		NODE_SET_METHOD(exports, "StartRecordingEncoderTest", autoConfig::StartRecordingEncoderTest);
		NODE_SET_METHOD(exports, "StartCheckSettings", autoConfig::StartCheckSettings);
		NODE_SET_METHOD(exports, "StartSetDefaultSettings", autoConfig::StartSetDefaultSettings);
		NODE_SET_METHOD(exports, "StartSaveStreamSettings", autoConfig::StartSaveStreamSettings);
		NODE_SET_METHOD(exports, "StartSaveSettings", autoConfig::StartSaveSettings);
		NODE_SET_METHOD(exports, "TerminateAutoConfig", autoConfig::TerminateAutoConfig);
	});
}