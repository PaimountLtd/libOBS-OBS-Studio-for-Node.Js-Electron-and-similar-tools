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

#pragma once
#include <napi.h>
#include "utility-v8.hpp"
#include "cache-manager.hpp"
#include "server/osn-source.hpp"

#undef strtoll
#include "nlohmann/json.hpp"

namespace osn
{
	struct SourceHotkeyInfo
	{
		std::string sourceName;
		std::string hotkeyName;
		std::string hotkeyDesc;
		size_t		hotkeyId;
	};

	class ISource
	{
		public:
		static void Release(obs_source_t* source);
		static void Remove(const Napi::CallbackInfo& info, obs_source_t* source);
		static void Update(const Napi::CallbackInfo& info, obs_source_t* source);
		static void Load(const Napi::CallbackInfo& info, obs_source_t* source);
		static void Save(const Napi::CallbackInfo& info, obs_source_t* source);

		static Napi::Value IsConfigurable(const Napi::CallbackInfo& info, obs_source_t* source);
		static Napi::Value GetProperties(const Napi::CallbackInfo& info, obs_source_t* source);
		static Napi::Value GetSettings(const Napi::CallbackInfo& info, obs_source_t* source);

		static Napi::Value GetType(const Napi::CallbackInfo& info, obs_source_t* source);
		static Napi::Value GetName(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SetName(const Napi::CallbackInfo& info, const Napi::Value &value, obs_source_t* source);
		static Napi::Value GetOutputFlags(const Napi::CallbackInfo& info, obs_source_t* source);
		static Napi::Value GetFlags(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SetFlags(const Napi::CallbackInfo& info, const Napi::Value &value, obs_source_t* source);
		static Napi::Value GetStatus(const Napi::CallbackInfo& info, obs_source_t* source);
		static Napi::Value GetId(const Napi::CallbackInfo& info, obs_source_t* source);
		static Napi::Value GetMuted(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SetMuted(const Napi::CallbackInfo& info, const Napi::Value &value, obs_source_t* source);
		static Napi::Value GetEnabled(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SetEnabled(const Napi::CallbackInfo& info, const Napi::Value &value, obs_source_t* source);

		static void SendMouseClick(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SendMouseMove(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SendMouseWheel(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SendFocus(const Napi::CallbackInfo& info, obs_source_t* source);
		static void SendKeyClick(const Napi::CallbackInfo& info, obs_source_t* source);
	};
}
