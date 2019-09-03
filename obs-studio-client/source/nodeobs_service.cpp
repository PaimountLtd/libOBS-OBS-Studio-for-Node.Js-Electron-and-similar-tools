/******************************************************************************
    Copyright (C) 2016-2019 by Streamlabs (General Workings Inc)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#include "nodeobs_service.hpp"
#include "controller.hpp"
#include "error.hpp"
#include "utility-v8.hpp"

#include <node.h>
#include <sstream>
#include <string>
#include "shared.hpp"
#include "utility.hpp"

Service::Service(){};
Service::~Service(){};

bool isWorkerRunning = false;

void Service::start_async_runner()
{
	if (m_async_callback)
		return;
	std::unique_lock<std::mutex> ul(m_worker_lock);
	// Start v8/uv asynchronous runner.
	m_async_callback = new ServiceCallback();
	m_async_callback->set_handler(std::bind(&Service::callback_handler, this, std::placeholders::_1, std::placeholders::_2), nullptr);
}
void Service::stop_async_runner()
{
	if (!m_async_callback)
		return;
	std::unique_lock<std::mutex> ul(m_worker_lock);
	// Stop v8/uv asynchronous runner.
	m_async_callback->clear();
	m_async_callback->finalize();
	m_async_callback = nullptr;
}

void Service::callback_handler(void* data, std::shared_ptr<SignalInfo> item)
{
	v8::Isolate*         isolate = v8::Isolate::GetCurrent();
	v8::Local<v8::Value> args[1];
	Nan::HandleScope     scope;

	v8::Local<v8::Value> argv = v8::Object::New(isolate);
	argv->ToObject()->Set(
	    v8::String::NewFromUtf8(isolate, "type").ToLocalChecked(),
	    v8::String::NewFromUtf8(isolate, item->outputType.c_str()).ToLocalChecked());
	argv->ToObject()->Set(
	    v8::String::NewFromUtf8(isolate, "signal").ToLocalChecked(),
	    v8::String::NewFromUtf8(isolate, item->signal.c_str()).ToLocalChecked());
	argv->ToObject()->Set(
	    v8::String::NewFromUtf8(isolate, "code").ToLocalChecked(), v8::Number::New(isolate, item->code));
	argv->ToObject()->Set(
	    v8::String::NewFromUtf8(isolate, "error").ToLocalChecked(),
	    v8::String::NewFromUtf8(isolate, item->errorMessage.c_str()).ToLocalChecked());
	args[0] = argv;

	Nan::Call(m_callback_function, 1, args);
}
void Service::start_worker()
{
	if (!m_worker_stop)
		return;
	// Launch worker thread.
	m_worker_stop = false;
	m_worker      = std::thread(std::bind(&Service::worker, this));
}
void Service::stop_worker()
{
	if (m_worker_stop != false)
		return;
	// Stop worker thread.
	m_worker_stop = true;
	if (m_worker.joinable()) {
		m_worker.join();
	}
}

void service::OBS_service_resetAudioContext(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_resetAudioContext", {});
}

void service::OBS_service_resetVideoContext(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_resetVideoContext", {});
}

void service::OBS_service_startStreaming(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	// Callback
	if (!isWorkerRunning) {
		serviceObject->start_async_runner();
		serviceObject->set_keepalive(args.This());
		serviceObject->start_worker();

		isWorkerRunning = true;
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_startStreaming", {});
}

void service::OBS_service_startRecording(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	// Callback
	if (!isWorkerRunning) {
		serviceObject->start_async_runner();
		serviceObject->set_keepalive(args.This());
		serviceObject->start_worker();

		isWorkerRunning = true;
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_startRecording", {});
}

void service::OBS_service_startReplayBuffer(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	// Callback
	if (!isWorkerRunning) {
		serviceObject->start_async_runner();
		serviceObject->set_keepalive(args.This());
		serviceObject->start_worker();

		isWorkerRunning = true;
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_startReplayBuffer", {});
}

void service::OBS_service_stopStreaming(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	bool forceStop;
	ASSERT_GET_VALUE(args[0], forceStop);

	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_stopStreaming", {ipc::value(forceStop)});
}

void service::OBS_service_stopRecording(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_stopRecording", {});
}

void service::OBS_service_stopReplayBuffer(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	bool forceStop;
	ASSERT_GET_VALUE(args[0], forceStop);

	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Service", "OBS_service_stopReplayBuffer", {ipc::value(forceStop)});
}

static v8::Persistent<v8::Object> serviceCallbackObject;

void service::OBS_service_connectOutputSignals(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::Local<v8::Function> callback;
	ASSERT_GET_VALUE(args[0], callback);

	// Grab IPC Connection
	std::shared_ptr<ipc::client> conn = nullptr;
	if (!(conn = GetConnection())) {
		return;
	}

	// Send request
	conn->call("Service", "OBS_service_connectOutputSignals", {});

	serviceObject = new Service();
	serviceObject->m_callback_function.Reset(callback);
	args.GetReturnValue().Set(true);
}

void service::OBS_service_processReplayBufferHotkey(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto conn = GetConnection();
	if (!conn)
		return;

    conn->call("Service", "OBS_service_processReplayBufferHotkey", {});
}

void service::OBS_service_getLastReplay(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response =
	    conn->call_synchronous_helper("Service", "OBS_service_getLastReplay", {});

	ValidateResponse(response);

	args.GetReturnValue().Set(
	    v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), response.at(1).value_str.c_str()).ToLocalChecked());
}

void Service::worker()
{
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
			std::vector<ipc::value> response = conn->call_synchronous_helper("Service", "Query", {});
			if (!response.size() || (response.size() == 1)) {
				goto do_sleep;
			}

			ErrorCode error = (ErrorCode)response[0].value_union.ui64;
			if (error == ErrorCode::Ok) {
				std::shared_ptr<SignalInfo> data = std::make_shared<SignalInfo>();

				data->outputType   = response[1].value_str;
				data->signal       = response[2].value_str;
				data->code         = response[3].value_union.i32;
				data->errorMessage = response[4].value_str;
				data->param        = this;

				m_async_callback->queue(std::move(data));
			}
		}

	do_sleep:
		auto tp_end  = std::chrono::high_resolution_clock::now();
		auto dur     = std::chrono::duration_cast<std::chrono::milliseconds>(tp_end - tp_start);
		totalSleepMS = sleepIntervalMS - dur.count();
		std::this_thread::sleep_for(std::chrono::milliseconds(totalSleepMS));
	}
	return;
}

void Service::set_keepalive(v8::Local<v8::Object> obj)
{
	if (!m_async_callback)
		return;
	m_async_callback->set_keepalive(obj);
}

void service::OBS_service_removeCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	if (isWorkerRunning) {
		serviceObject->stop_worker();
		serviceObject->stop_async_runner();
	}
}

INITIALIZER(nodeobs_service)
{
	initializerFunctions.push([](v8::Local<v8::Object> exports) {
		NODE_SET_METHOD(exports, "OBS_service_resetAudioContext", service::OBS_service_resetAudioContext);

		NODE_SET_METHOD(exports, "OBS_service_resetVideoContext", service::OBS_service_resetVideoContext);

		NODE_SET_METHOD(exports, "OBS_service_startStreaming", service::OBS_service_startStreaming);

		NODE_SET_METHOD(exports, "OBS_service_startRecording", service::OBS_service_startRecording);

		NODE_SET_METHOD(exports, "OBS_service_startReplayBuffer", service::OBS_service_startReplayBuffer);

		NODE_SET_METHOD(exports, "OBS_service_stopRecording", service::OBS_service_stopRecording);

		NODE_SET_METHOD(exports, "OBS_service_stopStreaming", service::OBS_service_stopStreaming);

		NODE_SET_METHOD(exports, "OBS_service_stopReplayBuffer", service::OBS_service_stopReplayBuffer);

		NODE_SET_METHOD(exports, "OBS_service_connectOutputSignals", service::OBS_service_connectOutputSignals);

		NODE_SET_METHOD(exports, "OBS_service_removeCallback", service::OBS_service_removeCallback);

		NODE_SET_METHOD(exports, "OBS_service_processReplayBufferHotkey", service::OBS_service_processReplayBufferHotkey);

		NODE_SET_METHOD(exports, "OBS_service_getLastReplay", service::OBS_service_getLastReplay);
	});
}