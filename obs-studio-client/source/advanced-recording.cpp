/******************************************************************************
    Copyright (C) 2016-2022 by Streamlabs (General Workings Inc)

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

#include "advanced-recording.hpp"
#include "utility.hpp"

Napi::FunctionReference osn::AdvancedRecording::constructor;

Napi::Object osn::AdvancedRecording::Init(Napi::Env env, Napi::Object exports) {
	Napi::HandleScope scope(env);
	Napi::Function func =
		DefineClass(env,
		"AdvancedRecording",
		{
            StaticMethod("create", &osn::AdvancedRecording::Create),

            InstanceAccessor(
                "path",
                &osn::AdvancedRecording::GetPath,
                &osn::AdvancedRecording::SetPath),
            InstanceAccessor(
                "format",
                &osn::AdvancedRecording::GetFormat,
                &osn::AdvancedRecording::SetFormat),
            InstanceAccessor(
                "muxerSettings",
                &osn::AdvancedRecording::GetMuxerSettings,
                &osn::AdvancedRecording::SetMuxerSettings),
            InstanceAccessor(
                "fileFormat",
                &osn::AdvancedRecording::GetFileFormat,
                &osn::AdvancedRecording::SetFileFormat),
            InstanceAccessor(
                "overwrite",
                &osn::AdvancedRecording::GetOverwrite,
                &osn::AdvancedRecording::SetOverwrite),
            InstanceAccessor(
                "noSpace",
                &osn::AdvancedRecording::GetNoSpace,
                &osn::AdvancedRecording::SetNoSpace),

            InstanceAccessor(
                "videoEncoder",
                &osn::AdvancedRecording::GetVideoEncoder,
                &osn::AdvancedRecording::SetVideoEncoder),
            InstanceAccessor(
                "signalHandler",
                &osn::AdvancedRecording::GetSignalHandler,
                &osn::AdvancedRecording::SetSignalHandler),
            InstanceAccessor(
                "mixer",
                &osn::AdvancedRecording::GetMixer,
                &osn::AdvancedRecording::SetMixer),
            InstanceAccessor(
                "rescaling",
                &osn::AdvancedRecording::GetRescaling,
                &osn::AdvancedRecording::SetRescaling),
            InstanceAccessor(
                "outputWidth",
                &osn::AdvancedRecording::GetOutputWidth,
                &osn::AdvancedRecording::SetOutputWidth),
            InstanceAccessor(
                "outputHeight",
                &osn::AdvancedRecording::GetOutputHeight,
                &osn::AdvancedRecording::SetOutputHeight),

			InstanceMethod("start", &osn::AdvancedRecording::Start),
			InstanceMethod("stop", &osn::AdvancedRecording::Stop)
		});

	exports.Set("AdvancedRecording", func);
	osn::AdvancedRecording::constructor = Napi::Persistent(func);
	osn::AdvancedRecording::constructor.SuppressDestruct();

	return exports;
}

osn::AdvancedRecording::AdvancedRecording(const Napi::CallbackInfo& info)
	: Napi::ObjectWrap<osn::AdvancedRecording>(info) {
	Napi::Env env = info.Env();
	Napi::HandleScope scope(env);
	int length = info.Length();

	if (length <= 0 || !info[0].IsNumber()) {
		Napi::TypeError::New(env, "Number expected").ThrowAsJavaScriptException();
		return;
	}

	this->uid = (uint64_t)info[0].ToNumber().Int64Value();
    this->className = std::string("AdvancedRecording");
}

Napi::Value osn::AdvancedRecording::Create(const Napi::CallbackInfo& info) {
	auto conn = GetConnection(info);
	if (!conn)
		return info.Env().Undefined();

	std::vector<ipc::value> response =
		conn->call_synchronous_helper("AdvancedRecording", "Create", {});

	if (!ValidateResponse(info, response))
		return info.Env().Undefined();

	auto instance =
		osn::AdvancedRecording::constructor.New({
			Napi::Number::New(info.Env(), response[1].value_union.ui64)
		});

	return instance;
}

Napi::Value osn::AdvancedRecording::GetMixer(const Napi::CallbackInfo& info) {
	auto conn = GetConnection(info);
	if (!conn)
		return info.Env().Undefined();

	std::vector<ipc::value> response =
		conn->call_synchronous_helper(
			"AdvancedRecording",
			"GetMixer",
			{ipc::value(this->uid)});

	if (!ValidateResponse(info, response))
		return info.Env().Undefined();

	return Napi::Number::New(info.Env(), response[1].value_union.ui32);
}

void osn::AdvancedRecording::SetMixer(const Napi::CallbackInfo& info, const Napi::Value& value) {
	auto conn = GetConnection(info);
	if (!conn)
		return;

	conn->call_synchronous_helper(
		"AdvancedRecording",
		"SetMixer",
		{ipc::value(this->uid), ipc::value(value.ToNumber().Uint32Value())});
}

Napi::Value osn::AdvancedRecording::GetRescaling(const Napi::CallbackInfo& info) {
	auto conn = GetConnection(info);
	if (!conn)
		return info.Env().Undefined();

	std::vector<ipc::value> response =
		conn->call_synchronous_helper(
			"AdvancedRecording",
			"GetRescaling",
			{ipc::value(this->uid)});

	if (!ValidateResponse(info, response))
		return info.Env().Undefined();

	return Napi::Boolean::New(info.Env(), response[1].value_union.ui32);
}

void osn::AdvancedRecording::SetRescaling(const Napi::CallbackInfo& info, const Napi::Value& value) {
	auto conn = GetConnection(info);
	if (!conn)
		return;

	conn->call_synchronous_helper(
		"AdvancedRecording",
		"SetRescaling",
		{ipc::value(this->uid), ipc::value(value.ToNumber().Uint32Value())});
}

Napi::Value osn::AdvancedRecording::GetOutputWidth(const Napi::CallbackInfo& info) {
	auto conn = GetConnection(info);
	if (!conn)
		return info.Env().Undefined();

	std::vector<ipc::value> response =
		conn->call_synchronous_helper(
			"AdvancedRecording",
			"GetOutputWidth",
			{ipc::value(this->uid)});

	if (!ValidateResponse(info, response))
		return info.Env().Undefined();

	return Napi::Number::New(info.Env(), response[1].value_union.ui32);
}

void osn::AdvancedRecording::SetOutputWidth(const Napi::CallbackInfo& info, const Napi::Value& value) {
	auto conn = GetConnection(info);
	if (!conn)
		return;

	conn->call_synchronous_helper(
		"AdvancedRecording",
		"SetOutputWidth",
		{ipc::value(this->uid), ipc::value(value.ToNumber().Uint32Value())});
}

Napi::Value osn::AdvancedRecording::GetOutputHeight(const Napi::CallbackInfo& info) {
	auto conn = GetConnection(info);
	if (!conn)
		return info.Env().Undefined();

	std::vector<ipc::value> response =
		conn->call_synchronous_helper(
			"AdvancedRecording",
			"GetOutputHeight",
			{ipc::value(this->uid)});

	if (!ValidateResponse(info, response))
		return info.Env().Undefined();

	return Napi::Number::New(info.Env(), response[1].value_union.ui32);
}

void osn::AdvancedRecording::SetOutputHeight(const Napi::CallbackInfo& info, const Napi::Value& value) {
	auto conn = GetConnection(info);
	if (!conn)
		return;

	conn->call_synchronous_helper(
		"AdvancedRecording",
		"SetOutputHeight",
		{ipc::value(this->uid), ipc::value(value.ToNumber().Uint32Value())});
}