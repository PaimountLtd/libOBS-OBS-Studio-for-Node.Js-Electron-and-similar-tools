#include "Scene.h"
#include "SceneItem.h"
#include "Input.h"
#include "Calldata.h"

namespace osn
{

typedef common::CallbackData<callback_data, Scene> SceneSignalCallback;

obs::source Scene::GetHandle()
{
	return handle.get().get();
}

Scene::Scene(std::string name, bool is_private)
	: handle(obs::scene(name, is_private))
{
}

Scene::Scene(obs::scene handle)
	: handle(obs::scene(handle))
{
}

Nan::Persistent<v8::FunctionTemplate> Scene::prototype =
      Nan::Persistent<v8::FunctionTemplate>();

NAN_MODULE_INIT(Scene::Init)
{
	SceneSignalCallback::Init(target);

	auto locProto = Nan::New<v8::FunctionTemplate>();
	locProto->Inherit(Nan::New(ISource::prototype));
	locProto->SetClassName(FIELD_NAME("Scene"));
	locProto->InstanceTemplate()->SetInternalFieldCount(1);

	common::SetObjectTemplateLazyAccessor(locProto->InstanceTemplate(), "source",
	                                      get_source);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "moveItem",
	                               moveItem);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "findItem",
	                               findItem);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "getItemAtIdx",
	                               getItemAtIdx);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "getItems",
	                               getItems);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "add", add);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "duplicate",
	                               duplicate);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "connect",
	                               connect);
	common::SetObjectTemplateField(locProto->InstanceTemplate(), "disconnect",
	                               disconnect);
	common::SetObjectTemplateField(locProto, "create", create);
	common::SetObjectTemplateField(locProto, "createPrivate", createPrivate);
	common::SetObjectTemplateField(locProto, "fromName", fromName);

	Nan::Set(target, FIELD_NAME("Scene"), locProto->GetFunction());
	prototype.Reset(locProto);
}

NAN_METHOD(Scene::get_source)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());

	Input *binding = new Input(scene.get()->source());
	auto object = Input::Object::GenerateObject(binding);

	info.GetReturnValue().Set(object);
}

NAN_METHOD(Scene::create)
{
	ASSERT_INFO_LENGTH(info, 1);

	std::string name;

	ASSERT_GET_VALUE(info[0], name);

	Scene *binding = new Scene(name);
	auto object = Scene::Object::GenerateObject(binding);
	info.GetReturnValue().Set(object);
}

NAN_METHOD(Scene::createPrivate)
{
	ASSERT_INFO_LENGTH(info, 1);

	std::string name;

	ASSERT_GET_VALUE(info[0], name);

	Scene *binding = new Scene(name, true);
	auto object = Scene::Object::GenerateObject(binding);
	info.GetReturnValue().Set(object);
}

NAN_METHOD(Scene::duplicate)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());

	ASSERT_INFO_LENGTH(info, 2);

	std::string name;
	int duplicate_type;

	ASSERT_GET_VALUE(info[0], name);
	ASSERT_GET_VALUE(info[1], duplicate_type);

	obs::scene dup_scene =
	      scene.get()->duplicate(name,
	                             static_cast<obs_scene_duplicate_type>(duplicate_type));

	if (dup_scene.status() != obs::scene::status_type::okay) {
		info.GetReturnValue().Set(Nan::Null());
		return;
	}

	Scene *binding = new Scene(dup_scene);
	auto object = Scene::Object::GenerateObject(binding);
	info.GetReturnValue().Set(object);
}

NAN_METHOD(Scene::fromName)
{
	ASSERT_INFO_LENGTH(info, 1);

	std::string name;

	ASSERT_GET_VALUE(info[0], name);

	obs::scene scene = obs::scene::from_name(name);

	if (scene.status() != obs::scene::status_type::okay) {
		info.GetReturnValue().Set(Nan::Null());
		return;
	}

	Scene *binding = new Scene(scene);
	auto object = Scene::Object::GenerateObject(binding);
	info.GetReturnValue().Set(object);
}

struct ItemMoveData {
	int count;
	int old_index;
	int new_index;
};

NAN_METHOD(Scene::moveItem)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());

	/* FIXME Race condition from iterating twice */
	ASSERT_INFO_LENGTH(info, 2);

	auto items = scene.get()->items();

	ItemMoveData move_data;

	ASSERT_GET_VALUE(info[0], move_data.old_index);
	ASSERT_GET_VALUE(info[1], move_data.new_index);
	move_data.count = static_cast<int>(items.size());

	move_data.old_index = (move_data.count - 1) - move_data.old_index;

	auto item_enum_cb =
	[] (obs_scene_t *scene, obs_sceneitem_t *item, void *data) {
		ItemMoveData *move_data =
		      reinterpret_cast<ItemMoveData *>(data);

		if (move_data->old_index == 0) {
			obs::scene::item(item)
			.order_position((move_data->count - 1) - move_data->new_index);

			return false;
		}

		move_data->old_index--;
		return true;
	};

	obs_scene_enum_items(scene.get().get().dangerous_scene(), item_enum_cb,
	                     &move_data);
}

NAN_METHOD(Scene::findItem)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());

	ASSERT_INFO_LENGTH_AT_LEAST(info, 1);

	obs::scene::item item;

	if (info[0]->IsString()) {
		std::string name;
		ASSERT_GET_VALUE(info[0], name);

		item = scene.get()->find_item(name);
	} else if (info[0]->IsNumber()) {
		int64_t position;
		ASSERT_GET_VALUE(info[0], position);

		item = scene.get()->find_item(position);
	} else {
		Nan::TypeError("Expected string or number");
		return;
	}

	if (item.status() != obs::source::status_type::okay) {
		info.GetReturnValue().Set(Nan::Null());
		return;
	}

	SceneItem *si_binding = new SceneItem(item);
	auto object = SceneItem::Object::GenerateObject(si_binding);

	info.GetReturnValue().Set(object);
}

NAN_METHOD(Scene::getItemAtIdx)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());
	std::vector<obs::scene::item> items = scene.get()->items();

	if (items.size() == 0) {
		info.GetReturnValue().Set(Nan::Null());
		return;
	}

	int64_t index;

	ASSERT_GET_VALUE(info[0], index);

	index = (items.size() - 1) - index;

	if (index < 0) {
		info.GetReturnValue().Set(Nan::Null());
		return;
	}

	SceneItem *binding = new SceneItem(items[index]);
	auto object = SceneItem::Object::GenerateObject(binding);
	info.GetReturnValue().Set(object);
}

NAN_METHOD(Scene::getItems)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());
	std::vector<obs::scene::item> items = scene.get()->items();
	int size = static_cast<int>(items.size());
	auto array = Nan::New<v8::Array>(size);

	for (int i = 0; i < size; ++i) {
		SceneItem *binding = new SceneItem(items[i]);
		auto object = SceneItem::Object::GenerateObject(binding);
		Nan::Set(array, i, object);
	}

	info.GetReturnValue().Set(array);
}

NAN_METHOD(Scene::add)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());

	ASSERT_INFO_LENGTH(info, 1);

	v8::Local<v8::Object> input_obj;

	ASSERT_GET_VALUE(info[0], input_obj);

	obs::weak<obs::input> &input =
	      Input::Object::GetHandle(input_obj);

	SceneItem *binding = new SceneItem(scene.get()->add(input.get().get()));
	auto object = SceneItem::Object::GenerateObject(binding);

	info.GetReturnValue().Set(object);
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
static const char *signal_type_map[] = {
	"item_add",
	"item_remove",
	"reorder",
	"item_visible",
	"item_select",
	"item_deselect",
	"item_transform"
};

enum signal_types {
	SIG_ITEM_ADD,
	SIG_ITEM_REMOVE,
	SIG_REORDER,
	SIG_ITEM_VISIBLE,
	SIG_ITEM_SELECT,
	SIG_ITEM_DESELECT,
	SIG_ITEM_TRANSORM,
	SIG_TYPE_OVERFLOW
};

static calldata_desc scene_signal_desc[] = {
	{ "scene", CALLDATA_TYPE_SCENE },
	{ "", CALLDATA_TYPE_END }
};

static calldata_desc item_signal_desc[] = {
	{ "scene", CALLDATA_TYPE_SCENE },
	{ "item", CALLDATA_TYPE_SCENEITEM },
	{ "", CALLDATA_TYPE_END }
};

static calldata_desc item_visible_signal_desc[] = {
	{ "scene", CALLDATA_TYPE_SCENE },
	{ "item", CALLDATA_TYPE_SCENEITEM },
	{ "visibility", CALLDATA_TYPE_BOOL },
	{ "", CALLDATA_TYPE_END }
};


static calldata_desc *callback_desc_map[] = {
	item_signal_desc,
	item_signal_desc,
	scene_signal_desc,
	item_visible_signal_desc,
	item_signal_desc,
	item_signal_desc,
	item_signal_desc
};

NAN_METHOD(Scene::connect)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());
	Scene *this_binding = Nan::ObjectWrap::Unwrap<Scene>(info.Holder());

	uint32_t signal_type;
	v8::Local<v8::Function> callback;

	ASSERT_GET_VALUE(info[0], signal_type);
	ASSERT_GET_VALUE(info[1], callback);

	if (signal_type >= SIG_TYPE_OVERFLOW || signal_type < 0) {
		Nan::ThrowError("Detected signal type out of range");
		return;
	}

	SceneSignalCallback *cb_binding =
	      new SceneSignalCallback(
	      this_binding,
	      CalldataEventHandler<Scene, callback_data, SceneSignalCallback>,
	      callback);

	cb_binding->user_data =
	      callback_desc_map[signal_type];

	scene.get()->connect(
	      signal_type_map[signal_type],
	      GenericSignalHandler<SceneSignalCallback>,
	      cb_binding);

	auto object = SceneSignalCallback::Object::GenerateObject(cb_binding);
	cb_binding->obj_ref.Reset(object);
	info.GetReturnValue().Set(object);
}

NAN_METHOD(Scene::disconnect)
{
	obs::weak<obs::scene> &scene = Scene::Object::GetHandle(info.Holder());

	uint32_t signal_type;
	v8::Local<v8::Object> cb_data_object;

	ASSERT_GET_VALUE(info[0], signal_type);
	ASSERT_GET_VALUE(info[1], cb_data_object);

	if (signal_type >= SIG_TYPE_OVERFLOW || signal_type < 0) {
		Nan::ThrowError("Detected signal type out of range");
		return;
	}

	SceneSignalCallback *cb_binding =
	      SceneSignalCallback::Object::GetHandle(cb_data_object);

	cb_binding->stopped = true;
	cb_binding->obj_ref.Reset();

	scene.get()->disconnect(
	      signal_type_map[signal_type],
	      GenericSignalHandler<SceneSignalCallback>,
	      cb_binding);
}

Nan::Persistent<v8::FunctionTemplate> SceneSignalCallback::prototype =
      Nan::Persistent<v8::FunctionTemplate>();
}