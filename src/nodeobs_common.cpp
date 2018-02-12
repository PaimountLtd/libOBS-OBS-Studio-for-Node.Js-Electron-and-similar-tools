#include "nodeobs_content.h"
#include <map>
#include <iomanip>

/* For sceneitem transform modifications.
 * We should consider moving this to another module */
#include <graphics/matrix4.h>

std::map<std::string, OBS::Display *> displays;
std::string sourceSelected;

/* A lot of the sceneitem functionality is a lazy copy-pasta from the Qt UI. */
// https://github.com/jp9000/obs-studio/blob/master/UI/window-basic-main.cpp#L4888
static void GetItemBox(obs_sceneitem_t *item, vec3 &tl, vec3 &br)
{
	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
	vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

	auto GetMinPos = [&] (float x, float y) {
		vec3 pos;
		vec3_set(&pos, x, y, 0.0f);
		vec3_transform(&pos, &pos, &boxTransform);
		vec3_min(&tl, &tl, &pos);
		vec3_max(&br, &br, &pos);
	};

	GetMinPos(0.0f, 0.0f);
	GetMinPos(1.0f, 0.0f);
	GetMinPos(0.0f, 1.0f);
	GetMinPos(1.0f, 1.0f);
}

static vec3 GetItemTL(obs_sceneitem_t *item)
{
	vec3 tl, br;
	GetItemBox(item, tl, br);
	return tl;
}

static void SetItemTL(obs_sceneitem_t *item, const vec3 &tl)
{
	vec3 newTL;
	vec2 pos;

	obs_sceneitem_get_pos(item, &pos);
	newTL = GetItemTL(item);
	pos.x += tl.x - newTL.x;
	pos.y += tl.y - newTL.y;
	obs_sceneitem_set_pos(item, &pos);
}

static bool CenterAlignSelectedItems(obs_scene_t *scene, obs_sceneitem_t *item,
                                     void *param)
{
	obs_bounds_type boundsType = *reinterpret_cast<obs_bounds_type *>(param);

	if (!obs_sceneitem_selected(item))
		return true;

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	obs_transform_info itemInfo;
	vec2_set(&itemInfo.pos, 0.0f, 0.0f);
	vec2_set(&itemInfo.scale, 1.0f, 1.0f);
	itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	itemInfo.rot = 0.0f;

	vec2_set(&itemInfo.bounds,
	         float(ovi.base_width), float(ovi.base_height));
	itemInfo.bounds_type = boundsType;
	itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

	obs_sceneitem_set_info(item, &itemInfo);

	UNUSED_PARAMETER(scene);
	return true;
}


static bool MultiplySelectedItemScale(obs_scene_t *scene, obs_sceneitem_t *item,
                                      void *param)
{
	vec2 &mul = *reinterpret_cast<vec2 *>(param);

	if (!obs_sceneitem_selected(item))
		return true;

	vec3 tl = GetItemTL(item);

	vec2 scale;
	obs_sceneitem_get_scale(item, &scale);
	vec2_mul(&scale, &scale, &mul);
	obs_sceneitem_set_scale(item, &scale);

	SetItemTL(item, tl);

	UNUSED_PARAMETER(scene);
	return true;
}

void OBS_content::OBS_content_createDisplay(const FunctionCallbackInfo<Value>
                                            &args)
{
	Isolate *isolate = args.GetIsolate();
	v8::EscapableHandleScope scope(isolate);

	v8::Local<v8::Object> bufferObj = args[0].As<v8::Object>();
	unsigned char *bufferData = (unsigned char *)node::Buffer::Data(bufferObj);
	uint64_t windowHandle = *reinterpret_cast<uint64_t *>(bufferData);

	v8::String::Utf8Value key(args[1]);

	auto found = displays.find(*key);

	/* If found, do nothing since it would
	   be a memory leak otherwise. */
	if (found != displays.end()) {
		std::cerr << "Duplicate key provided to createDisplay: " << *key << std::endl;
		return;
	}

	displays[*key] = new OBS::Display(windowHandle);
}

void OBS_content::OBS_content_destroyDisplay(const FunctionCallbackInfo<Value>
                                             &args)
{
	Isolate *isolate = args.GetIsolate();

	v8::String::Utf8Value key(args[0]);

	auto found = displays.find(*key);

	if (found == displays.end()) {
		std::cerr << "Failed to find key for destruction: " << *key << std::endl;
		return;
	}

	delete found->second;
	displays.erase(found);
}

void OBS_content::OBS_content_createSourcePreviewDisplay(
      const FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();
	v8::EscapableHandleScope scope(isolate);

	v8::Local<v8::Object> bufferObj = args[0].As<v8::Object>();
	unsigned char *bufferData = (unsigned char *)node::Buffer::Data(bufferObj);
	uint64_t windowHandle = *reinterpret_cast<uint64_t *>(bufferData);

	v8::String::Utf8Value sourceName(args[1]->ToString());
	v8::String::Utf8Value key(args[2]);

	auto found = displays.find(*key);

	/* If found, do nothing since it would
	   be a memory leak otherwise. */
	if (found != displays.end()) {
		std::cout << "Duplicate key provided to createDisplay!" << std::endl;
		return;
	}
	displays[*key] = new OBS::Display(windowHandle, *sourceName);
}

void OBS_content::OBS_content_resizeDisplay(const FunctionCallbackInfo<Value>
                                            &args)
{
	Isolate *isolate = args.GetIsolate();

	v8::String::Utf8Value key(args[0]);

	auto value = displays.find(*key);
	if (value == displays.end()) {
		std::cout << "Invalid key provided to resizeDisplay: " << *key << std::endl;
		return;
	}

	OBS::Display *display = value->second;

	int width = args[1]->ToUint32()->Value();
	int height = args[2]->ToUint32()->Value();

	display->SetSize(width, height);
}

void OBS_content::OBS_content_moveDisplay(const FunctionCallbackInfo<Value>
                                          &args)
{
	Isolate *isolate = args.GetIsolate();

	v8::String::Utf8Value key(args[0]);

	auto value = displays.find(*key);
	if (value == displays.end()) {
		std::cout << "Invalid key provided to moveDisplay: " << *key << std::endl;
		return;
	}

	OBS::Display *display = value->second;

	int x = args[1]->ToUint32()->Value();
	int y = args[2]->ToUint32()->Value();

	display->SetPosition(x, y);
}

void OBS_content::OBS_content_setPaddingSize(const FunctionCallbackInfo<Value>
                                             &args)
{
	Isolate *isolate = args.GetIsolate();

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate,
		                                    "Usage: OBS_content_setPaddingSize(displayKey<string>, size<number>)")
		      )
		);
		return;
	case 1:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{size} is not a <number>!")
		      )
		);
		return;
	}

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetPaddingSize(args[1]->Int32Value());
	return;
}

void OBS_content::OBS_content_setPaddingColor(const
                                              FunctionCallbackInfo<Value> &args)
{
	union {
		uint32_t rgba;
		uint8_t c[4];
	} color;
	Isolate *isolate = args.GetIsolate();

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate,
		                                    "Usage: OBS_content_setPaddingColor(displayKey<string>, red<number>{0.0, 255.0}, green<number>{0.0, 255.0}, blue<number>{0.0, 255.0}[, alpha<number>{0.0, 1.0}])")
		      )
		);
		return;
	case 1:
	case 2:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{red} is not a <number>!")
		      )
		);
		return;
	}
	if (args[2]->IsUndefined() && !args[2]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{green} is not a <number>!")
		      )
		);
		return;
	}
	if (args[3]->IsUndefined() && !args[3]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{blue} is not a <number>!")
		      )
		);
		return;
	}

	// Assign Color
	color.c[0] = (uint8_t)(args[1]->NumberValue());
	color.c[1] = (uint8_t)(args[2]->NumberValue());
	color.c[2] = (uint8_t)(args[3]->NumberValue());
	if (!args[4]->IsUndefined() && args[4]->IsNumber())
		color.c[3] = (uint8_t)(args[4]->NumberValue() * 255.0);

	else
		color.c[3] = 255;

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetPaddingColor(color.c[0], color.c[1], color.c[2], color.c[3]);
	return;
}

void OBS_content::OBS_content_setBackgroundColor(const
                                                 FunctionCallbackInfo<Value> &args)
{
	union {
		uint32_t rgba;
		uint8_t c[4];
	} color;
	Isolate *isolate = args.GetIsolate();

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate,
		                                    "Usage: OBS_content_setBackgroundColor(displayKey<string>, red<number>{0.0, 255.0}, green<number>{0.0, 255.0}, blue<number>{0.0, 255.0}[, alpha<number>{0.0, 1.0}])")
		      )
		);
		return;
	case 1:
	case 2:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{red} is not a <number>!")
		      )
		);
		return;
	}
	if (args[2]->IsUndefined() && !args[2]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{green} is not a <number>!")
		      )
		);
		return;
	}
	if (args[3]->IsUndefined() && !args[3]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{blue} is not a <number>!")
		      )
		);
		return;
	}

	// Assign Color
	color.c[0] = (uint8_t)(args[1]->NumberValue());
	color.c[1] = (uint8_t)(args[2]->NumberValue());
	color.c[2] = (uint8_t)(args[3]->NumberValue());
	if (!args[4]->IsUndefined() && args[4]->IsNumber())
		color.c[3] = (uint8_t)(args[4]->NumberValue() * 255.0);

	else
		color.c[3] = 255;

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetBackgroundColor(color.c[0], color.c[1], color.c[2], color.c[3]);
	return;
}

void OBS_content::OBS_content_setOutlineColor(const
                                              FunctionCallbackInfo<Value> &args)
{
	union {
		uint32_t rgba;
		uint8_t c[4];
	} color;
	Isolate *isolate = args.GetIsolate();

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate,
		                                    "Usage: OBS_content_setOutlineColor(displayKey<string>, red<number>{0.0, 255.0}, green<number>{0.0, 255.0}, blue<number>{0.0, 255.0}[, alpha<number>{0.0, 1.0}])")
		      )
		);
		return;
	case 1:
	case 2:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{red} is not a <number>!")
		      )
		);
		return;
	}
	if (args[2]->IsUndefined() && !args[2]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{green} is not a <number>!")
		      )
		);
		return;
	}
	if (args[3]->IsUndefined() && !args[3]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{blue} is not a <number>!")
		      )
		);
		return;
	}

	// Assign Color
	color.c[0] = (uint8_t)(args[1]->NumberValue());
	color.c[1] = (uint8_t)(args[2]->NumberValue());
	color.c[2] = (uint8_t)(args[3]->NumberValue());
	if (!args[4]->IsUndefined() && args[4]->IsNumber())
		color.c[3] = (uint8_t)(args[4]->NumberValue() * 255.0);

	else
		color.c[3] = 255;

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetOutlineColor(color.c[0], color.c[1], color.c[2], color.c[3]);
	return;
}

void OBS_content::OBS_content_setGuidelineColor(const
                                                FunctionCallbackInfo<Value> &args)
{
	union {
		uint32_t rgba;
		uint8_t c[4];
	} color;
	Isolate *isolate = args.GetIsolate();

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate,
		                                    "Usage: OBS_content_setGuidelineColor(displayKey<string>, red<number>{0.0, 255.0}, green<number>{0.0, 255.0}, blue<number>{0.0, 255.0}[, alpha<number>{0.0, 1.0}])")
		      )
		);
		return;
	case 1:
	case 2:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{red} is not a <number>!")
		      )
		);
		return;
	}
	if (args[2]->IsUndefined() && !args[2]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{green} is not a <number>!")
		      )
		);
		return;
	}
	if (args[3]->IsUndefined() && !args[3]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{blue} is not a <number>!")
		      )
		);
		return;
	}

	// Assign Color
	color.c[0] = (uint8_t)(args[1]->NumberValue());
	color.c[1] = (uint8_t)(args[2]->NumberValue());
	color.c[2] = (uint8_t)(args[3]->NumberValue());
	if (!args[4]->IsUndefined() && args[4]->IsNumber())
		color.c[3] = (uint8_t)(args[4]->NumberValue() * 255.0);

	else
		color.c[3] = 255;

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetGuidelineColor(color.c[0], color.c[1], color.c[2], color.c[3]);
	return;
}

void OBS_content::OBS_content_setResizeBoxOuterColor(const
                                                     FunctionCallbackInfo<Value> &args)
{
	union {
		uint32_t rgba;
		uint8_t c[4];
	} color;
	Isolate *isolate = args.GetIsolate();

	const char *usage_string =
		"Usage: OBS_content_setResizeBoxOuterColor"
		"(displayKey<string>, red<number>{0.0, 255.0}, "
		"green<number>{0.0, 255.0}, blue<number>{0.0, 255.0}"
		"[, alpha<number>{0.0, 1.0}])";


	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, usage_string)
			)
		);
		return;
	case 1:
	case 2:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{red} is not a <number>!")
		      )
		);
		return;
	}
	if (args[2]->IsUndefined() && !args[2]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{green} is not a <number>!")
		      )
		);
		return;
	}
	if (args[3]->IsUndefined() && !args[3]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{blue} is not a <number>!")
		      )
		);
		return;
	}

	// Assign Color
	color.c[0] = (uint8_t)(args[1]->NumberValue());
	color.c[1] = (uint8_t)(args[2]->NumberValue());
	color.c[2] = (uint8_t)(args[3]->NumberValue());
	if (!args[4]->IsUndefined() && args[4]->IsNumber())
		color.c[3] = (uint8_t)(args[4]->NumberValue() * 255.0);

	else
		color.c[3] = 255;

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetResizeBoxOuterColor(color.c[0], color.c[1], color.c[2],
	                                   color.c[3]);
	return;
}

void OBS_content::OBS_content_setResizeBoxInnerColor(const
                                                     FunctionCallbackInfo<Value> &args)
{
	union {
		uint32_t rgba;
		uint8_t c[4];
	} color;
	Isolate *isolate = args.GetIsolate();

	const char *usage_string = 
		"Usage: OBS_content_setResizeBoxInnerColor"
		"(displayKey<string>, red<number>{0.0, 255.0},"
		" green<number>{0.0, 255.0}, blue<number>{0.0, 255.0}"
		"[, alpha<number>{0.0, 1.0}])";


	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, usage_string)
			)
		);
		return;
	case 1:
	case 2:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{red} is not a <number>!")
		      )
		);
		return;
	}
	if (args[2]->IsUndefined() && !args[2]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{green} is not a <number>!")
		      )
		);
		return;
	}
	if (args[3]->IsUndefined() && !args[3]->IsNumber()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{blue} is not a <number>!")
		      )
		);
		return;
	}

	// Assign Color
	color.c[0] = (uint8_t)(args[1]->NumberValue());
	color.c[1] = (uint8_t)(args[2]->NumberValue());
	color.c[2] = (uint8_t)(args[3]->NumberValue());
	if (!args[4]->IsUndefined() && args[4]->IsNumber())
		color.c[3] = (uint8_t)(args[4]->NumberValue() * 255.0);

	else
		color.c[3] = 255;

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetResizeBoxInnerColor(color.c[0], color.c[1], color.c[2],
	                                   color.c[3]);
	return;
}

void OBS_content::OBS_content_setShouldDrawUI(const
                                              FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();

	const char *usage_string = 
		"Usage: OBS_content_setShouldDrawUI"
		"(displayKey<string>, value<boolean>)";

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, usage_string)
		      )
		);
		return;
	case 1:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "Not enough Parameters")
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}
	if (args[1]->IsUndefined() && !args[1]->IsBoolean()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{value} is not a <boolean>!")
		      )
		);
		return;
	}

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	it->second->SetDrawUI(args[1]->BooleanValue());
}

void OBS_content::OBS_content_getDisplayPreviewOffset(const
                                                      FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();
	v8::EscapableHandleScope handle(isolate);

	v8::String::Utf8Value key(args[0]);

	auto value = displays.find(*key);
	if (value == displays.end()) {
		std::cout << "Invalid key provided to moveDisplay: " << *key << std::endl;
		return;
	}

	OBS::Display *display = value->second;

	auto offset = display->GetPreviewOffset();

	v8::Local<v8::Object> object = v8::Object::New(isolate);
	object->Set(v8::String::NewFromUtf8(isolate, "x"), v8::Int32::New(isolate,
	                                                                  offset.first));
	object->Set(v8::String::NewFromUtf8(isolate, "y"), v8::Int32::New(isolate,
	                                                                  offset.second));

	args.GetReturnValue().Set(handle.Escape(object));
}

void OBS_content::OBS_content_getDisplayPreviewSize(const
                                                    FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();
	v8::EscapableHandleScope handle(isolate);

	v8::String::Utf8Value key(args[0]);

	auto value = displays.find(*key);
	if (value == displays.end()) {
		std::cout << "Invalid key provided to moveDisplay: " << *key << std::endl;
		return;
	}

	OBS::Display *display = value->second;

	auto size = display->GetPreviewSize();

	v8::Local<v8::Object> object = v8::Object::New(isolate);
	object->Set(v8::String::NewFromUtf8(isolate, "width"), v8::Int32::New(isolate,
	                                                                      size.first));
	object->Set(v8::String::NewFromUtf8(isolate, "height"), v8::Int32::New(isolate,
	                                                                       size.second));

	args.GetReturnValue().Set(handle.Escape(object));
}

/* Deprecated */
void OBS_content::OBS_content_selectSource(const FunctionCallbackInfo<Value>
                                           &args)
{
	/* Here we assume that channel 0 holds the one and only transition.
	 * We also assume that the active source within that transition is
	 * the scene that we need */
	obs_source_t *transition = obs_get_output_source(0);
	obs_source_t *source = obs_transition_get_active_source(transition);
	obs_scene_t *scene = obs_scene_from_source(source);

	obs_source_release(transition);

	int x = args[0]->NumberValue();
	int y = args[1]->NumberValue();

	auto function = [] (obs_scene_t *, obs_sceneitem_t *item,
	void *listSceneItems) {
		vector<obs_sceneitem_t * > &items =
		      *reinterpret_cast<vector<obs_sceneitem_t *>*>(listSceneItems);

		items.push_back(item);
		return true;
	};

	vector<obs_sceneitem_t *> listSceneItems;
	obs_scene_enum_items(scene, function, &listSceneItems);

	bool sourceFound = false;

	for (int i = 0; i < listSceneItems.size(); ++i) {
		obs_sceneitem_t *item = listSceneItems[i];
		obs_source_t *source = obs_sceneitem_get_source(item);
		const char *sourceName = obs_source_get_name(source);

		struct vec2 position;
		obs_sceneitem_get_pos(item, &position);

		int positionX = position.x;
		int positionY = position.y;

		int width = obs_source_get_width(source);
		int height = obs_source_get_height(source);

		if(x >= positionX && x <= width + positionX &&
		            y >= positionY && y < height + positionY) {
			sourceSelected = sourceName;
			sourceFound = true;
			break;
		}
	}

	if(!sourceFound) {
		sourceSelected = "";
		cout << "source not found !!!!" << endl;
	}

	obs_source_release(source);
}

/* Deprecated */
bool selectItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	vector<std::string> &sources = *reinterpret_cast<vector<std::string>*>(param);

	obs_source_t *source = obs_sceneitem_get_source(item);
	std::string name = obs_source_get_name(source);

	if(std::find(sources.begin(), sources.end(), name) != sources.end())
		obs_sceneitem_select(item, true);

	else
		obs_sceneitem_select(item, false);
	return true;
}

/* Deprecated */
void OBS_content::OBS_content_selectSources(const FunctionCallbackInfo<Value> &args)
{
	v8::Local<v8::Array> sources = v8::Local<v8::Array>::Cast(args[0]);

	obs_source_t *transition = obs_get_output_source(0);
	obs_source_t *source = obs_transition_get_active_source(transition);
	obs_scene_t *scene = obs_scene_from_source(source);

	obs_source_release(transition);

	vector<std::string> tabSources;

	{
		for(int i = 0; i < sources->Length(); i++) {
			v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(sources->Get(i));
			v8::String::Utf8Value value(object->Get(Nan::New("name").ToLocalChecked()));
			std::string sourceName = std::string(*value);

			tabSources.push_back(sourceName);
		}

		if (scene)
			obs_scene_enum_items(scene, selectItems, &tabSources);
	}

	obs_source_release(source);
}

void OBS_content::OBS_content_dragSelectedSource(const FunctionCallbackInfo<Value> &args)
{
	int x = args[0]->NumberValue();
	int y = args[1]->NumberValue();

	if(sourceSelected.compare("") ==0)
		return;

	if(x < 0)
		x = 0;

	if(y < 0)
		y = 0;

	obs_source_t *transition = obs_get_output_source(0);
	obs_source_t *source = obs_transition_get_active_source(transition);
	obs_scene_t *scene = obs_scene_from_source(source);

	obs_source_release(transition);

	obs_sceneitem_t *sourceItem = 
		obs_scene_find_source(scene, sourceSelected.c_str());

	struct vec2 position;
	position.x = x;
	position.y = y;

	obs_sceneitem_set_pos(sourceItem, &position);
	obs_source_release(source);
}

void OBS_content::OBS_content_getDrawGuideLines(const
                                                FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();

	const char *usage_string =
		"Usage: OBS_content_getDrawGuideLines(displayKey<string>)";

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, usage_string)
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);
		return;
	}

	args.GetReturnValue().Set(
		Boolean::New(isolate, it->second->GetDrawGuideLines()));
}

void OBS_content::OBS_content_setDrawGuideLines(const FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();

	const char *usage_string =
		"Usage: OBS_content_getDrawGuideLines"
		"(displayKey<string>, drawGuideLines<boolean>)";

	// Validate Arguments
	/// Amount
	switch (args.Length()) {
	case 0:
	case 1:
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, usage_string)
		      )
		);
		return;
	}

	/// Types
	if (args[0]->IsUndefined() && !args[0]->IsString()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not a <string>!")
		      )
		);
		return;
	}

	if (args[1]->IsUndefined() && !args[1]->IsBoolean()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{size} is not a <boolean>!")
		      )
		);
		return;
	}

	// Find Display
	v8::String::Utf8Value key(args[0]->ToString());
	auto it = displays.find(*key);
	if (it == displays.end()) {
		isolate->ThrowException(
		      v8::Exception::SyntaxError(
		            v8::String::NewFromUtf8(isolate, "{displayKey} is not valid!")
		      )
		);

		return;
	}

	it->second->SetDrawGuideLines(args[1]->BooleanValue());
}
