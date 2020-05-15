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

#include "scene.hpp"
#include <condition_variable>
#include <mutex>
#include <string>
#include "controller.hpp"
#include "error.hpp"
#include "input.hpp"
#include "ipc-value.hpp"
#include "sceneitem.hpp"
#include "shared.hpp"
#include "utility.hpp"

osn::Scene::Scene(uint64_t id)
{
	this->sourceId = id;
}

Nan::Persistent<v8::FunctionTemplate> osn::Scene::prototype = Nan::Persistent<v8::FunctionTemplate>();

void osn::Scene::Register(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target)
{
	auto fnctemplate = Nan::New<v8::FunctionTemplate>();
	fnctemplate->Inherit(Nan::New<v8::FunctionTemplate>(osn::ISource::prototype));
	fnctemplate->InstanceTemplate()->SetInternalFieldCount(1);
	fnctemplate->SetClassName(Nan::New<v8::String>("Scene").ToLocalChecked());

	// Class Template
	utilv8::SetTemplateField(fnctemplate, "create", Create);
	utilv8::SetTemplateField(fnctemplate, "createPrivate", CreatePrivate);
	utilv8::SetTemplateField(fnctemplate, "fromName", FromName);

	// Prototype/Class Template
	v8::Local<v8::ObjectTemplate> objtemplate = fnctemplate->PrototypeTemplate();
	utilv8::SetTemplateField(objtemplate, "release", Release);
	utilv8::SetTemplateField(objtemplate, "remove", Remove);

	utilv8::SetTemplateAccessorProperty(objtemplate, "source", AsSource);
	utilv8::SetTemplateField(objtemplate, "duplicate", Duplicate);
	utilv8::SetTemplateField(objtemplate, "add", AddSource);
	utilv8::SetTemplateField(objtemplate, "findItem", FindItem);
	utilv8::SetTemplateField(objtemplate, "moveItem", MoveItem);
	utilv8::SetTemplateField(objtemplate, "orderItems", OrderItems);
	utilv8::SetTemplateField(objtemplate, "getItemAtIdx", GetItemAtIndex);
	utilv8::SetTemplateField(objtemplate, "getItems", GetItems);
	utilv8::SetTemplateField(objtemplate, "getItemsInRange", GetItemsInRange);
	utilv8::SetTemplateField(objtemplate, "connect", Connect);
	utilv8::SetTemplateField(objtemplate, "disconnect", Disconnect);

	// Stuff
	utilv8::SetObjectField(
	    target, "Scene", fnctemplate->GetFunction(target->GetIsolate()->GetCurrentContext()).ToLocalChecked());
	prototype.Reset(fnctemplate);
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::Create(Nan::NAN_METHOD_ARGS_TYPE info)
{
	std::string name;

	ASSERT_INFO_LENGTH(info, 1);
	ASSERT_GET_VALUE(info[0], name);

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response =
	    conn->call_synchronous_helper("Scene", "Create", std::vector<ipc::value>{ipc::value(name)});

	if (!ValidateResponse(response))
		return;

	uint64_t sourceId = response[1].value_union.ui64;

	osn::Scene* obj   = new osn::Scene(sourceId);

	SceneInfo* si       = new SceneInfo();
	si->name            = name;
	si->id              = sourceId;
	SourceDataInfo* sdi = new SourceDataInfo;
	sdi->name           = name;
	sdi->obs_sourceId   = "scene";
	sdi->id             = response[1].value_union.ui64;

	CacheManager<SourceDataInfo*>::getInstance().Store(sourceId, name, sdi);
	CacheManager<SceneInfo*>::getInstance().Store(sourceId, name, si);

	info.GetReturnValue().Set(osn::Scene::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::CreatePrivate(Nan::NAN_METHOD_ARGS_TYPE info)
{
	std::string name;

	ASSERT_INFO_LENGTH(info, 1);
	ASSERT_GET_VALUE(info[0], name);

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response =
	    conn->call_synchronous_helper("Scene", "CreatePrivate", std::vector<ipc::value>{ipc::value(name)});

	if (!ValidateResponse(response))
		return;

	uint64_t sourceId = response[1].value_union.ui64;

	osn::Scene* obj   = new osn::Scene(sourceId);

	SceneInfo* si       = new SceneInfo();
	si->name            = name;
	si->id              = sourceId;
	SourceDataInfo* sdi = new SourceDataInfo;
	sdi->name           = name;
	sdi->obs_sourceId   = "scene";
	sdi->id             = response[1].value_union.ui64;

	CacheManager<SourceDataInfo*>::getInstance().Store(sourceId, name, sdi);
	CacheManager<SceneInfo*>::getInstance().Store(sourceId, name, si);

	info.GetReturnValue().Set(osn::Scene::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::FromName(Nan::NAN_METHOD_ARGS_TYPE info)
{
	std::string name;

	ASSERT_INFO_LENGTH(info, 1);
	ASSERT_GET_VALUE(info[0], name);

	SceneInfo* si = CacheManager<SceneInfo*>::getInstance().Retrieve(name);

	if (!si) {
		auto conn = GetConnection();
		if (!conn)
			return;

		std::vector<ipc::value> response = conn->call_synchronous_helper("Scene", "FromName", {ipc::value(name)});

		if (!ValidateResponse(response))
			return;

		si = new SceneInfo;
		si->id = response[1].value_union.ui64;
		si->name = name;
		CacheManager<SceneInfo*>::getInstance().Store(response[1].value_union.ui64, name, si);
	}
	osn::Scene* obj = new osn::Scene(si->id);
	info.GetReturnValue().Set(osn::Scene::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::Release(Nan::NAN_METHOD_ARGS_TYPE info)
{
	osn::Scene* source = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), source)) {
		return;
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Scene", "Release", std::vector<ipc::value>{ipc::value(source->sourceId)});
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::Remove(Nan::NAN_METHOD_ARGS_TYPE info)
{
	osn::Scene* source = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), source)) {
		return;
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	conn->call("Scene", "Remove", std::vector<ipc::value>{ipc::value(source->sourceId)});
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::AsSource(Nan::NAN_METHOD_ARGS_TYPE info)
{
	// Scenes are simply stored as a normal source object on the server, no additional calls necessary.
	osn::Scene* source = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), source)) {
		return;
	}

	osn::Input* obj = new osn::Input(source->sourceId);
	info.GetReturnValue().Set(osn::Input::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::Duplicate(Nan::NAN_METHOD_ARGS_TYPE info)
{
	std::string name;
	int         duplicate_type;
	osn::Scene* source = nullptr;

	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), source)) {
		return;
	}

	ASSERT_INFO_LENGTH(info, 2);
	ASSERT_GET_VALUE(info[0], name);
	ASSERT_GET_VALUE(info[1], duplicate_type);

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response = conn->call_synchronous_helper(
	    "Scene",
	    "Duplicate",
	    std::vector<ipc::value>{ipc::value(source->sourceId), ipc::value(name), ipc::value(duplicate_type)});

	if (!ValidateResponse(response))
		return;

	uint64_t sourceId = response[1].value_union.ui64;

	osn::Scene* obj = new osn::Scene(sourceId);

	SourceDataInfo* sdi = new SourceDataInfo;
	sdi->name           = name;
	sdi->obs_sourceId   = "scene";
	sdi->id             = response[1].value_union.ui64;

	CacheManager<SourceDataInfo*>::getInstance().Store(sourceId, name, sdi);
	CacheManager<SceneInfo*>::getInstance().Store(sourceId, name, new SceneInfo);

	info.GetReturnValue().Set(osn::Scene::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::AddSource(Nan::NAN_METHOD_ARGS_TYPE info)
{
	std::vector<ipc::value> params;
	osn::Scene* scene = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), scene)) {
		return;
	}

	osn::Input* input = nullptr;
	if (info.Length() >= 1) {
		if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Input>(info[0]->ToObject(), input)) {
			return;
		}
	}
	params.push_back(ipc::value(scene->sourceId));
	params.push_back(ipc::value(input->sourceId));
	v8::Local<v8::Object> transform = v8::Object::New(v8::Isolate::GetCurrent());
	v8::Local<v8::Object> crop      = v8::Object::New(v8::Isolate::GetCurrent());
	
	transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("scaleX"))
	    .ToLocalChecked()
	    ->ToNumber(info.GetIsolate()->GetCurrentContext()).ToLocalChecked()->Value();

	if (info.Length() >= 2) {
		transform = info[1]->ToObject();
		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("scaleX"))
		                                .ToLocalChecked()
		                                ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                                .ToLocalChecked()
		                                ->Value()
		));

		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("scaleY"))
		                                .ToLocalChecked()
		                                ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                                .ToLocalChecked()
		                                ->Value()));
		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("visible"))
		                                .ToLocalChecked()
		                                ->ToBoolean()
		                                ->Value()));
		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("x"))
		                                .ToLocalChecked()
		                                ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                                .ToLocalChecked()
		                                ->Value()));
		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("y"))
		                                .ToLocalChecked()
		                                ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                                .ToLocalChecked()
		                                ->Value()));
		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("rotation"))
		                                .ToLocalChecked()
		                                ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                                .ToLocalChecked()
		                                ->Value()));

		crop = transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("crop"))
		           .ToLocalChecked()
		           ->ToObject();
		params.push_back(ipc::value(crop->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("left"))
		                                .ToLocalChecked()
		                                ->ToInteger()
		                                ->Value()));
		params.push_back(ipc::value(crop->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("top"))
		                                .ToLocalChecked()
		                                ->ToInteger()
		                                ->Value()));
		params.push_back(ipc::value(crop->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("right"))
		                                .ToLocalChecked()
		                                ->ToInteger()
		                                ->Value()));
		params.push_back(ipc::value(crop->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("bottom"))
		                                .ToLocalChecked()
		                                ->ToInteger()
		                                ->Value()));

		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("streamVisible"))
										.ToLocalChecked()
										->ToBoolean()
										->Value()));
		params.push_back(ipc::value(transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("recordingVisible"))
										.ToLocalChecked()
										->ToBoolean()
										->Value()));
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response = conn->call_synchronous_helper(
	    "Scene", "AddSource", params);

	if (!ValidateResponse(response))
		return;

	uint64_t id     = response[1].value_union.ui64;
	int64_t  obs_id = response[2].value_union.i64;
	// Create new SceneItem
	osn::SceneItem* obj = new osn::SceneItem(id);
	SceneInfo*      si  = CacheManager<SceneInfo*>::getInstance().Retrieve(scene->sourceId);

	if (si) {
		si->items.push_back(std::make_pair(obs_id, id));
		si->itemsOrderCached = true;
	}

	SceneItemData* sid = new SceneItemData;
	sid->obs_itemId    = obs_id;
	sid->scene_id      = scene->sourceId;

	if (info.Length() >= 2) {
		// Position
		sid->posX = transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("x"))
		                .ToLocalChecked()
		                ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                .ToLocalChecked()
		                ->Value();
		sid->posY = transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("y"))
		                .ToLocalChecked()
		                ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                .ToLocalChecked()
		                ->Value();
		sid->posChanged = false;

		// Scale
		sid->scaleX = transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("scaleX"))
		                  .ToLocalChecked()
		                  ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                  .ToLocalChecked()
		                  ->Value();
		sid->scaleY = transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("scaleY"))
		                  .ToLocalChecked()
		                  ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                  .ToLocalChecked()
		                  ->Value();
		sid->scaleChanged = false;

		// Visibility
		sid->isVisible      = transform->Get(utilv8::ToValue("visible"))->ToBoolean()->Value();
		sid->visibleChanged = false;

		// Crop
		sid->cropLeft    = crop->Get(utilv8::ToValue("left"))->ToInteger()->Value();
		sid->cropTop     = crop->Get(utilv8::ToValue("top"))->ToInteger()->Value();
		sid->cropRight   = crop->Get(utilv8::ToValue("right"))->ToInteger()->Value();
		sid->cropBottom  = crop->Get(utilv8::ToValue("bottom"))->ToInteger()->Value();
		sid->cropChanged = false;

		// Rotation
		sid->rotation = transform->Get(info.GetIsolate()->GetCurrentContext(), utilv8::ToValue("rotation"))
		                    .ToLocalChecked()
		                    ->ToNumber(info.GetIsolate()->GetCurrentContext())
		                    .ToLocalChecked()
		                    ->Value();
		sid->rotationChanged = false;

		// Stream visible
		sid->isStreamVisible      = transform->Get(utilv8::ToValue("streamVisible"))->ToBoolean()->Value();
		sid->streamVisibleChanged = false;

		// Recording visible
		sid->isRecordingVisible      = transform->Get(utilv8::ToValue("recordingVisible"))->ToBoolean()->Value();
		sid->recordingVisibleChanged = false;
	}

	CacheManager<SceneItemData*>::getInstance().Store(id, sid);	

	info.GetReturnValue().Set(osn::SceneItem::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::FindItem(Nan::NAN_METHOD_ARGS_TYPE info)
{
	bool        haveName = false;
	std::string name;
	int64_t     position;

	osn::Scene* scene = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), scene)) {
		return;
	}

	ASSERT_INFO_LENGTH(info, 1);
	if (info[0]->IsNumber()) {
		haveName = false;
		ASSERT_GET_VALUE(info[0], position);
	} else if (info[0]->IsString()) {
		haveName = true;
		ASSERT_GET_VALUE(info[0], name);
	} else {
		Nan::TypeError("Expected string or number");
		return;
	}

	SceneInfo* si = CacheManager<SceneInfo*>::getInstance().Retrieve(scene->sourceId);

	if (si && !haveName) {
		auto find = [position](const std::pair<int64_t, uint64_t> &item) {
			return item.first == position;
		};

		auto itemIt = std::find_if(si->items.begin(), si->items.end(), find);
		if (itemIt != si->items.end()) {
			osn::SceneItem* obj = new osn::SceneItem(itemIt->second);
			info.GetReturnValue().Set(osn::SceneItem::Store(obj));
			return;
		}
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response = conn->call_synchronous_helper(
	    "Scene",
	    "FindItem",
	    std::vector<ipc::value>{ipc::value(scene->sourceId), (haveName ? ipc::value(name) : ipc::value(position))});

	if (!ValidateResponse(response))
		return;

	uint64_t id = response[1].value_union.ui64;

	osn::SceneItem* obj = new osn::SceneItem(id);
	info.GetReturnValue().Set(osn::SceneItem::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::MoveItem(Nan::NAN_METHOD_ARGS_TYPE info)
{
	osn::Scene* scene = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), scene)) {
		return;
	}

	int from, to;
	ASSERT_INFO_LENGTH(info, 2);
	ASSERT_GET_VALUE(info[0], from);
	ASSERT_GET_VALUE(info[1], to);

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response = conn->call_synchronous_helper(
	    "Scene", "MoveItem", std::vector<ipc::value>{ipc::value(scene->sourceId), ipc::value(from), ipc::value(to)});

	ValidateResponse(response);

	SceneInfo* si = CacheManager<SceneInfo*>::getInstance().Retrieve(scene->sourceId);

	if (si && response.size() > 2) {
		si->items.clear();

		for (size_t i = 1; i < response.size(); i += 2) {
			si->items.push_back(std::make_pair(response[i + 1].value_union.i64, response[i].value_union.ui64));
		}
		si->itemsOrderCached = true;
	}
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::OrderItems(Nan::NAN_METHOD_ARGS_TYPE info)
{
	osn::Scene* scene = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), scene)) {
		return;
	}

	std::vector<int64_t> order;
	std::vector<char> order_char;
	ASSERT_INFO_LENGTH(info, 1);

	v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(info[0]); 

	order.resize(array->Length());
	for (unsigned int i = 0; i < array->Length(); i++ ) {
		if (Nan::Has(array, i).FromJust()) {
			order[i] = Nan::Get(array, i).ToLocalChecked()->IntegerValue();
		}
	}
	order_char.resize(order.size()*sizeof(int64_t));
	memcpy(order_char.data(), order.data(), order_char.size());

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response = conn->call_synchronous_helper(
	    "Scene", "OrderItems", std::vector<ipc::value>{ipc::value(scene->sourceId), ipc::value(order_char)});

	ValidateResponse(response);

	SceneInfo* si = CacheManager<SceneInfo*>::getInstance().Retrieve(scene->sourceId);

	if (si && response.size() > 2) {
		si->items.clear();

		for (size_t i = 1; i < response.size(); i += 2) {
			si->items.push_back(std::make_pair(response[i + 1].value_union.i64, response[i].value_union.ui64));
		}
		si->itemsOrderCached = true;
	}
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::GetItemAtIndex(Nan::NAN_METHOD_ARGS_TYPE info)
{
	int32_t index;

	osn::Scene* scene = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), scene)) {
		return;
	}

	ASSERT_INFO_LENGTH(info, 1);
	ASSERT_GET_VALUE(info[0], index);

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response = conn->call_synchronous_helper(
	    "Scene", "GetItem", std::vector<ipc::value>{ipc::value(scene->sourceId), ipc::value(index)});

	if (!ValidateResponse(response))
		return;

	uint64_t id = response[1].value_union.ui64;

	osn::SceneItem* obj = new osn::SceneItem(id);
	info.GetReturnValue().Set(osn::SceneItem::Store(obj));
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::GetItems(Nan::NAN_METHOD_ARGS_TYPE info)
{
	osn::Scene* scene = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), scene)) {
		return;
	}

	SceneInfo* si = CacheManager<SceneInfo*>::getInstance().Retrieve(scene->sourceId);

	if (si && si->itemsOrderCached) {
		auto   arr         = Nan::New<v8::Array>(int(si->items.size()) - 1);
		size_t index = 0;
		bool   itemRemoved = false;

		for (auto item : si->items) {
			SceneItemData*  sid = CacheManager<SceneItemData*>::getInstance().Retrieve(item.first);
			if (!sid) {
				itemRemoved = true;
				break;
			}
			osn::SceneItem* obj = new osn::SceneItem(item.second);
			Nan::Set(arr, uint32_t(index++), osn::SceneItem::Store(obj));
		}
		if (!itemRemoved) {
			info.GetReturnValue().Set(arr);
			return;
		}
	}

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response =
	    conn->call_synchronous_helper("Scene", "GetItems", std::vector<ipc::value>{ipc::value(scene->sourceId)});

	if (!ValidateResponse(response))
		return;

	auto arr = Nan::New<v8::Array>(int((response.size()) - 1)/2);
	size_t index = 0;
	for (size_t i = 1; i < response.size(); i++) {
		osn::SceneItem* obj = new osn::SceneItem(response[i++].value_union.ui64);
		Nan::Set(arr, uint32_t(index++), osn::SceneItem::Store(obj));
	}

	info.GetReturnValue().Set(arr);

	if (si) {
		si->items.clear();

		for (size_t i = 1; i < response.size(); i+= 2) {
			si->items.push_back(std::make_pair(response[i + 1].value_union.i64, response[i].value_union.ui64));
		}

		si->itemsOrderCached = true;
	}
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::GetItemsInRange(Nan::NAN_METHOD_ARGS_TYPE info)
{
	osn::Scene* scene = nullptr;
	if (!utilv8::RetrieveDynamicCast<osn::ISource, osn::Scene>(info.This(), scene)) {
		return;
	}

	int32_t from, to;
	ASSERT_INFO_LENGTH(info, 2);
	ASSERT_GET_VALUE(info[0], from);
	ASSERT_GET_VALUE(info[1], to);

	auto conn = GetConnection();
	if (!conn)
		return;

	std::vector<ipc::value> response = conn->call_synchronous_helper(
	    "Scene",
	    "GetItemsInRange",
	    std::vector<ipc::value>{ipc::value(scene->sourceId), ipc::value(from), ipc::value(to)});

	if (!ValidateResponse(response))
		return;

	auto arr = Nan::New<v8::Array>(int(response.size() - 1));

	for (size_t i = 1; i < response.size(); i++) {
		osn::SceneItem* obj = new osn::SceneItem(response[i].value_union.ui64);
		Nan::Set(arr, uint32_t(i - 1), osn::SceneItem::Store(obj));
	}

	info.GetReturnValue().Set(arr);
}

/**
If libobs allowed us the ability to parse
or obtain info about the signals associated with
a handler, this could be done generically instead of a
hard coded table like this.

Notice that in this case, the signal handler of a scene
is in addition to the signals a source can receive.
However, I just require you use the signal handler
associated with the input object instead to keep things
simple.
*/
//static const char *signal_type_map[] = {
//	"item_add",
//	"item_remove",
//	"reorder",
//	"item_visible",
//	"item_select",
//	"item_deselect",
//	"item_transform"
//};
//
//enum signal_types {
//	SIG_ITEM_ADD,
//	SIG_ITEM_REMOVE,
//	SIG_REORDER,
//	SIG_ITEM_VISIBLE,
//	SIG_ITEM_SELECT,
//	SIG_ITEM_DESELECT,
//	SIG_ITEM_TRANSORM,
//	SIG_TYPE_OVERFLOW
//};
//
//static calldata_desc scene_signal_desc[] = {
//	{ "scene", CALLDATA_TYPE_SCENE },
//{ "", CALLDATA_TYPE_END }
//};
//
//static calldata_desc item_signal_desc[] = {
//	{ "scene", CALLDATA_TYPE_SCENE },
//{ "item", CALLDATA_TYPE_SCENEITEM },
//{ "", CALLDATA_TYPE_END }
//};
//
//static calldata_desc item_visible_signal_desc[] = {
//	{ "scene", CALLDATA_TYPE_SCENE },
//{ "item", CALLDATA_TYPE_SCENEITEM },
//{ "visibility", CALLDATA_TYPE_BOOL },
//{ "", CALLDATA_TYPE_END }
//};
//
//
//static calldata_desc *callback_desc_map[] = {
//	item_signal_desc,
//	item_signal_desc,
//	scene_signal_desc,
//	item_visible_signal_desc,
//	item_signal_desc,
//	item_signal_desc,
//	item_signal_desc
//};

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::Connect(Nan::NAN_METHOD_ARGS_TYPE info)
{
	//obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());
	//Scene* this_binding = Nan::ObjectWrap::Unwrap<Scene>(info.Holder());

	//uint32_t signal_type;
	//v8::Local<v8::Function> callback;

	//ASSERT_GET_VALUE(info[0], signal_type);
	//ASSERT_GET_VALUE(info[1], callback);

	//if (signal_type >= SIG_TYPE_OVERFLOW || signal_type < 0) {
	//	Nan::ThrowError("Detected signal type out of range");
	//	return;
	//}

	//SceneSignalCallback *cb_binding =
	//	new SceneSignalCallback(
	//		this_binding,
	//		CalldataEventHandler<Scene, callback_data, SceneSignalCallback>,
	//		callback);

	//cb_binding->user_data =
	//	callback_desc_map[signal_type];

	//scene.get()->connect(
	//	signal_type_map[signal_type],
	//	GenericSignalHandler<SceneSignalCallback>,
	//	cb_binding);

	//auto object = SceneSignalCallback::Object::GenerateObject(cb_binding);
	//cb_binding->obj_ref.Reset(object);
	//info.GetReturnValue().Set(object);
}

Nan::NAN_METHOD_RETURN_TYPE osn::Scene::Disconnect(Nan::NAN_METHOD_ARGS_TYPE info)
{
	//obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());

	//uint32_t signal_type;
	//v8::Local<v8::Object> cb_data_object;

	//ASSERT_GET_VALUE(info[0], signal_type);
	//ASSERT_GET_VALUE(info[1], cb_data_object);

	//if (signal_type >= SIG_TYPE_OVERFLOW || signal_type < 0) {
	//	Nan::ThrowError("Detected signal type out of range");
	//	return;
	//}

	//SceneSignalCallback *cb_binding =
	//	SceneSignalCallback::Object::GetHandle(cb_data_object);

	//cb_binding->stopped = true;
	//cb_binding->obj_ref.Reset();

	//scene.get()->disconnect(
	//	signal_type_map[signal_type],
	//	GenericSignalHandler<SceneSignalCallback>,
	//	cb_binding);
}
