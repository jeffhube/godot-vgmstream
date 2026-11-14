#include "audio_stream_awb_loader.h"

#include "audio_stream_awb.h"

void AudioStreamAWBLoader::_bind_methods() {
}

godot::PackedStringArray AudioStreamAWBLoader::_get_recognized_extensions() const {
	godot::PackedStringArray extensions;
	extensions.push_back("awb");
	return extensions;
}

bool AudioStreamAWBLoader::_recognize_path(const godot::String &p_path, const godot::StringName &p_type) const {
	const godot::StringName audio_stream_type("AudioStream");
	const godot::StringName awb_stream_type("AudioStreamAWB");
	const bool type_matches = p_type == godot::StringName() || p_type == audio_stream_type || p_type == awb_stream_type;
	return type_matches && p_path.get_extension().to_lower() == "awb";
}

bool AudioStreamAWBLoader::_handles_type(const godot::StringName &p_type) const {
	return p_type == godot::StringName("AudioStream") || p_type == godot::StringName("AudioStreamAWB");
}

godot::String AudioStreamAWBLoader::_get_resource_type(const godot::String &p_path) const {
	return p_path.get_extension().to_lower() == "awb" ? "AudioStreamAWB" : "";
}

godot::Variant AudioStreamAWBLoader::_load(const godot::String &p_path, const godot::String &, bool, int32_t) const {
	godot::Ref<AudioStreamAWB> stream;
	stream.instantiate();
	stream->source_path = p_path;
	return godot::Variant(stream);
}
