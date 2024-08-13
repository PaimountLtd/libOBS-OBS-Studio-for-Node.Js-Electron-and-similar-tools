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

#include "callback-manager.hpp"
#include "controller.hpp"
#include "osn-error.hpp"
#include "utility-v8.hpp"

#include <node.h>
#include <sstream>
#include <string>
#include "shared.hpp"
#include "utility.hpp"
#include "volmeter.hpp"

bool globalCallback::isWorkerRunning = false;
bool globalCallback::worker_stop = true;
uint32_t globalCallback::sleepIntervalMS = 50;
std::thread *globalCallback::worker_thread = nullptr;
Napi::ThreadSafeFunction globalCallback::js_source_callback;
Napi::ThreadSafeFunction globalCallback::js_volmeter_callback;
bool globalCallback::m_all_workers_stop = false;
std::mutex globalCallback::mtx_volmeters;
std::unordered_set<uint64_t> globalCallback::volmeters;

void globalCallback::Init(Napi::Env env, Napi::Object exports)
{
	exports.Set(Napi::String::New(env, "RegisterSourceCallback"), Napi::Function::New(env, globalCallback::RegisterSourceCallback));
	exports.Set(Napi::String::New(env, "RemoveSourceCallback"), Napi::Function::New(env, globalCallback::RemoveSourceCallback));

	exports.Set(Napi::String::New(env, "RegisterVolmeterCallback"), Napi::Function::New(env, globalCallback::RegisterVolmeterCallback));
	exports.Set(Napi::String::New(env, "RemoveVolmeterCallback"), Napi::Function::New(env, globalCallback::RemoveVolmeterCallback));
}

Napi::Value globalCallback::RegisterSourceCallback(const Napi::CallbackInfo &info)
{
	Napi::Function async_callback = info[0].As<Napi::Function>();

	start_worker(info.Env(), async_callback);
	isWorkerRunning = true;
	worker_stop = false;

	worker_thread = new std::thread(&globalCallback::worker);

	return Napi::Boolean::New(info.Env(), true);
}

Napi::Value globalCallback::RemoveSourceCallback(const Napi::CallbackInfo &info)
{
	if (isWorkerRunning)
		stop_worker();

	return info.Env().Undefined();
}

Napi::Value globalCallback::RegisterVolmeterCallback(const Napi::CallbackInfo &info)
{
	Napi::Function async_callback = info[0].As<Napi::Function>();
	js_volmeter_callback = Napi::ThreadSafeFunction::New(info.Env(), async_callback, "VolmeterCallback", 0, 1, [](Napi::Env) {});

	return Napi::Boolean::New(info.Env(), true);
}

Napi::Value globalCallback::RemoveVolmeterCallback(const Napi::CallbackInfo &info)
{
	js_volmeter_callback.Release();
	return info.Env().Undefined();
}

void globalCallback::start_worker(napi_env env, Napi::Function async_callback)
{
	if (!worker_stop)
		return;

	js_source_callback = Napi::ThreadSafeFunction::New(env, async_callback, "SourceCallback", 0, 1, [](Napi::Env) {});
}

void globalCallback::stop_worker(void)
{
	if (worker_stop != false)
		return;

	worker_stop = true;
	if (worker_thread->joinable()) {
		worker_thread->join();
	}
	js_source_callback.Release();
}

void globalCallback::worker()
{
	auto sources_callback = [](Napi::Env env, Napi::Function jsCallback, SourceSizeInfoData *data) {
		try {
			Napi::Array result = Napi::Array::New(env, data->items.size());

			for (size_t i = 0; i < data->items.size(); i++) {
				Napi::Object obj = Napi::Object::New(env);
				obj.Set("name", Napi::String::New(env, data->items[i]->name));
				obj.Set("width", Napi::Number::New(env, data->items[i]->width));
				obj.Set("height", Napi::Number::New(env, data->items[i]->height));
				obj.Set("flags", Napi::Number::New(env, data->items[i]->flags));
				result.Set(i, obj);
			}
			jsCallback.Call({result});
		} catch (...) {
		}
		delete data;
	};

	auto volmeter_callback = [](Napi::Env env, Napi::Function jsCallback, VolmeterDataArray *dataArray) {
		try {
			Napi::Array result = Napi::Array::New(env, dataArray->items.size());

			for (size_t i = 0; i < dataArray->items.size(); i++) {
				Napi::Object obj = Napi::Object::New(env);
				VolmeterData *item = dataArray->items[i].get();

				Napi::Array magnitude = Napi::Array::New(env);
				Napi::Array peak = Napi::Array::New(env);
				Napi::Array input_peak = Napi::Array::New(env);

				for (size_t j = 0; j < item->magnitude.size(); j++) {
					magnitude.Set(j, Napi::Number::New(env, item->magnitude[j]));
				}
				for (size_t j = 0; j < item->peak.size(); j++) {
					peak.Set(j, Napi::Number::New(env, item->peak[j]));
				}
				for (size_t j = 0; j < item->input_peak.size(); j++) {
					input_peak.Set(j, Napi::Number::New(env, item->input_peak[j]));
				}

				obj.Set("sourceName", Napi::String::New(env, item->source_name));
				obj.Set("magnitude", magnitude);
				obj.Set("peak", peak);
				obj.Set("inputPeak", input_peak);

				result.Set(i, obj);
			}

			jsCallback.Call({result});
		} catch (...) {
		}

		delete dataArray;
	};

	size_t totalSleepMS = 0;

	while (!worker_stop && !m_all_workers_stop) {
		auto tp_start = std::chrono::high_resolution_clock::now();

		auto conn = Controller::GetInstance().GetConnection();
		if (!conn)
			return;

		mtx_volmeters.lock();
		std::vector<char> volmeters_ids;
		{
			uint32_t index = 0;
			volmeters_ids.resize(sizeof(uint64_t) * volmeters.size());
			for (auto vol : volmeters) {
				*reinterpret_cast<uint64_t *>(volmeters_ids.data() + index) = vol;
				index += sizeof(uint64_t);
			}
		}

		{
			std::vector<ipc::value> response = conn->call_synchronous_helper(
				"CallbackManager", "GlobalQuery", {ipc::value((uint64_t)volmeters_ids.size()), ipc::value(volmeters_ids)});
			if (!response.size() || (response.size() == 1)) {
				goto do_sleep;
			}

			uint32_t index = 1;

			SourceSizeInfoData *data = new SourceSizeInfoData{{}};
			for (int i = 2; i < (response[1].value_union.ui32 * 4) + 2; i++) {
				SourceSizeInfo *item = new SourceSizeInfo;

				item->name = response[i++].value_str;
				item->width = response[i++].value_union.ui32;
				item->height = response[i++].value_union.ui32;
				item->flags = response[i].value_union.ui32;
				data->items.emplace_back(item);
				index = i;
			}

			if (data->items.size() > 0) {
				napi_status status = js_source_callback.NonBlockingCall(data, sources_callback);
				if (status != napi_ok) {
					delete data;
				}
			}

			index++;

			auto volmeterDataArray = new VolmeterDataArray;
			for (auto vol : volmeters) {
				VolmeterData *item = new VolmeterData{{}, {}, {}};

				item->source_name = response[index++].value_str;

				size_t channels = response[index++].value_union.i32;
				bool isMuted = response[index++].value_union.i32;

				if (!isMuted) {
					item->magnitude.resize(channels);
					item->peak.resize(channels);
					item->input_peak.resize(channels);
					for (size_t ch = 0; ch < channels; ch++) {
						item->magnitude[ch] = response[index + ch * 3 + 0].value_union.fp32;
						item->peak[ch] = response[index + ch * 3 + 1].value_union.fp32;
						item->input_peak[ch] = response[index + ch * 3 + 2].value_union.fp32;
					}

					index += (3 * channels);

					volmeterDataArray->items.emplace_back(item);
				}
			}

			if (js_volmeter_callback) {
				napi_status status = js_volmeter_callback.NonBlockingCall(volmeterDataArray, volmeter_callback);
				if (status != napi_ok) {
					delete volmeterDataArray;
				}
			}
		}

	do_sleep:
		mtx_volmeters.unlock();
		auto tp_end = std::chrono::high_resolution_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(tp_end - tp_start);
		totalSleepMS = sleepIntervalMS - dur.count();
		if (totalSleepMS > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(totalSleepMS));
	}
	return;
}

void globalCallback::add_volmeter(uint64_t id)
{
	volmeters.insert(id);
}

void globalCallback::remove_volmeter(uint64_t id)
{
	volmeters.erase(id);
}