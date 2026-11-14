#include "audio_stream_awb.h"

#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/string_name.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace godot;

namespace {

constexpr int32_t MIX_BUFFER_FRAMES = 4096;

struct MemoryStreamFile {
	libstreamfile_t stream{};
	std::shared_ptr<const std::vector<uint8_t>> data;
	std::string name;
};

int memory_stream_read(void *p_user_data, uint8_t *p_dst, int64_t p_offset, int p_length) {
	const auto *file = static_cast<const MemoryStreamFile *>(p_user_data);
	if (p_offset < 0 || p_length <= 0 || static_cast<uint64_t>(p_offset) >= file->data->size()) {
		return 0;
	}

	const size_t offset = static_cast<size_t>(p_offset);
	const size_t available = file->data->size() - offset;
	const size_t count = std::min(static_cast<size_t>(p_length), available);
	std::memcpy(p_dst, file->data->data() + offset, count);
	return static_cast<int>(count);
}

int64_t memory_stream_get_size(void *p_user_data) {
	const auto *file = static_cast<const MemoryStreamFile *>(p_user_data);
	return static_cast<int64_t>(file->data->size());
}

const char *memory_stream_get_name(void *p_user_data) {
	const auto *file = static_cast<const MemoryStreamFile *>(p_user_data);
	return file->name.c_str();
}

libstreamfile_t *memory_stream_open(void *p_user_data, const char *p_filename) {
	const auto *file = static_cast<const MemoryStreamFile *>(p_user_data);
	if (!p_filename || file->name != p_filename) {
		return nullptr;
	}

	auto *opened = new (std::nothrow) MemoryStreamFile{};
	if (!opened) {
		return nullptr;
	}

	opened->data = file->data;
	opened->name = p_filename;
	opened->stream.user_data = opened;
	opened->stream.read = memory_stream_read;
	opened->stream.get_size = memory_stream_get_size;
	opened->stream.get_name = memory_stream_get_name;
	opened->stream.open = memory_stream_open;
	opened->stream.close = [](libstreamfile_t *p_stream) {
		delete static_cast<MemoryStreamFile *>(p_stream->user_data);
	};
	return &opened->stream;
}

libstreamfile_t *create_memory_stream_file(std::shared_ptr<const std::vector<uint8_t>> p_data, const std::string &p_name) {
	auto *file = new (std::nothrow) MemoryStreamFile{};
	if (!file) {
		return nullptr;
	}

	file->data = std::move(p_data);
	file->name = p_name;
	file->stream.user_data = file;
	file->stream.read = memory_stream_read;
	file->stream.get_size = memory_stream_get_size;
	file->stream.get_name = memory_stream_get_name;
	file->stream.open = memory_stream_open;
	file->stream.close = [](libstreamfile_t *p_stream) {
		delete static_cast<MemoryStreamFile *>(p_stream->user_data);
	};
	return &file->stream;
}

std::string to_utf8(const godot::String &p_string) {
	const godot::CharString utf8 = p_string.utf8();
	return std::string(utf8.get_data(), static_cast<size_t>(utf8.length()));
}

} // namespace

void AudioStreamAWBPlayback::_bind_methods() {
}

AudioStreamAWBPlayback::~AudioStreamAWBPlayback() {
	if (decoder) {
		libvgmstream_free(decoder);
	}
}

bool AudioStreamAWBPlayback::open_stream(const godot::String &p_source_path, int32_t p_subsong_index) {
	if (decoder) {
		libvgmstream_free(decoder);
		decoder = nullptr;
	}
	playing = false;

	if (p_source_path.is_empty()) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: source path is empty.");
		return false;
	}
	if (p_subsong_index < 1) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: subsong_index must be at least 1.");
		return false;
	}

	godot::Ref<godot::FileAccess> file = godot::FileAccess::open(p_source_path, godot::FileAccess::READ);
	if (file.is_null() || !file->is_open()) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: could not open file: " + p_source_path);
		return false;
	}

	const uint64_t file_size = file->get_length();
	if (file_size == 0 || file_size > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: file is empty or too large: " + p_source_path);
		return false;
	}

	const godot::PackedByteArray bytes = file->get_buffer(static_cast<int64_t>(file_size));
	if (bytes.size() != static_cast<int64_t>(file_size)) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: failed to read file: " + p_source_path);
		return false;
	}

	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>(bytes.ptr(), bytes.ptr() + bytes.size());
	mix_buffer.resize(static_cast<size_t>(MIX_BUFFER_FRAMES) * 2);

	const std::string name = to_utf8(p_source_path);
	libstreamfile_t *stream_file = create_memory_stream_file(std::move(data), name);
	if (!stream_file) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: could not create a vgmstream input for: " + p_source_path);
		return false;
	}

	decoder = libvgmstream_init();
	if (!decoder) {
		libstreamfile_close(stream_file);
		godot::UtilityFunctions::push_error("AudioStreamAWB: could not initialize vgmstream.");
		return false;
	}

	libvgmstream_config_t config{};
	config.force_sfmt = LIBVGMSTREAM_SFMT_FLOAT;
	config.auto_downmix_channels = 2;
	libvgmstream_setup(decoder, &config);

	const int result = libvgmstream_open_stream(decoder, stream_file, p_subsong_index);
	libstreamfile_close(stream_file);
	if (result < 0) {
		libvgmstream_free(decoder);
		decoder = nullptr;
		godot::UtilityFunctions::push_error("AudioStreamAWB: vgmstream could not decode subsong " + godot::String::num_int64(p_subsong_index) + " from " + p_source_path);
		return false;
	}

	sample_rate = decoder->format->sample_rate;
	channels = decoder->format->channels;
	if (sample_rate <= 0 || (channels != 1 && channels != 2)) {
		libvgmstream_free(decoder);
		decoder = nullptr;
		godot::UtilityFunctions::push_error("AudioStreamAWB: vgmstream returned an unsupported audio format for: " + p_source_path);
		return false;
	}
	return true;
}

void AudioStreamAWBPlayback::_start(double p_from_pos) {
	if (!decoder) {
		playing = false;
		return;
	}

	libvgmstream_reset(decoder);
	libvgmstream_seek(decoder, static_cast<int64_t>(std::max(0.0, p_from_pos) * sample_rate));
	begin_resample();
	playing = true;
}

void AudioStreamAWBPlayback::_stop() {
	playing = false;
}

bool AudioStreamAWBPlayback::_is_playing() const {
	return playing && decoder && !decoder->decoder->done;
}

int32_t AudioStreamAWBPlayback::_get_loop_count() const {
	return 0;
}

double AudioStreamAWBPlayback::_get_playback_position() const {
	if (!decoder || sample_rate <= 0) {
		return 0.0;
	}
	const int64_t position = libvgmstream_get_play_position(decoder);
	return position > 0 ? static_cast<double>(position) / sample_rate : 0.0;
}

void AudioStreamAWBPlayback::_seek(double p_position) {
	if (decoder && sample_rate > 0) {
		libvgmstream_seek(decoder, static_cast<int64_t>(std::max(0.0, p_position) * sample_rate));
		begin_resample();
	}
}

int32_t AudioStreamAWBPlayback::_mix_resampled(godot::AudioFrame *p_buffer, int32_t p_frames) {
	if (!playing || !decoder || p_frames <= 0) {
		return 0;
	}

	int32_t mixed_frames = 0;
	while (mixed_frames < p_frames) {
		const int32_t frames_to_decode = std::min(MIX_BUFFER_FRAMES, p_frames - mixed_frames);
		if (libvgmstream_fill(decoder, mix_buffer.data(), frames_to_decode) < 0) {
			playing = false;
			godot::UtilityFunctions::push_error("AudioStreamAWB: vgmstream failed while decoding audio.");
			break;
		}

		const int32_t decoded_frames = decoder->decoder->buf_samples;
		const float *samples = mix_buffer.data();
		for (int32_t i = 0; i < decoded_frames; ++i) {
			const size_t sample_index = static_cast<size_t>(i) * channels;
			p_buffer[mixed_frames + i].left = samples[sample_index];
			p_buffer[mixed_frames + i].right = channels == 1 ? samples[sample_index] : samples[sample_index + 1];
		}
		mixed_frames += decoded_frames;

		if (decoder->decoder->done) {
			playing = false;
			break;
		}
		if (decoded_frames == 0) {
			break;
		}
	}

	return mixed_frames;
}

float AudioStreamAWBPlayback::_get_stream_sampling_rate() const {
	return static_cast<float>(sample_rate);
}

void AudioStreamAWB::_bind_methods() {
	using namespace godot;

	ClassDB::bind_method(D_METHOD("set_source_stream", "source_stream"), &AudioStreamAWB::set_source_stream);
	ClassDB::bind_method(D_METHOD("get_source_stream"), &AudioStreamAWB::get_source_stream);
	ClassDB::bind_method(D_METHOD("set_subsong_index", "subsong_index"), &AudioStreamAWB::set_subsong_index);
	ClassDB::bind_method(D_METHOD("get_subsong_index"), &AudioStreamAWB::get_subsong_index);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source_stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStreamAWB"), "set_source_stream", "get_source_stream");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subsong_index", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), "set_subsong_index", "get_subsong_index");
}

void AudioStreamAWB::set_source_stream(const godot::Ref<AudioStreamAWB> &p_source_stream) {
	if (p_source_stream.is_valid() && (p_source_stream.ptr() == this || p_source_stream->source_path.is_empty())) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: source_stream must reference an AWB file resource.");
		return;
	}
	source_stream = p_source_stream;
	emit_changed();
}

godot::Ref<AudioStreamAWB> AudioStreamAWB::get_source_stream() const {
	return source_stream;
}

void AudioStreamAWB::set_subsong_index(int32_t p_subsong_index) {
	subsong_index = p_subsong_index;
	emit_changed();
}

int32_t AudioStreamAWB::get_subsong_index() const {
	return subsong_index;
}

godot::Ref<godot::AudioStreamPlayback> AudioStreamAWB::_instantiate_playback() const {
	const godot::String &path = source_stream.is_valid() ? source_stream->source_path : source_path;
	if (path.is_empty()) {
		godot::UtilityFunctions::push_error("AudioStreamAWB: assign an AWB file resource before playback.");
		return godot::Ref<godot::AudioStreamPlayback>();
	}

	godot::Ref<AudioStreamAWBPlayback> playback;
	playback.instantiate();
	if (!playback->open_stream(path, subsong_index)) {
		return godot::Ref<godot::AudioStreamPlayback>();
	}
	return playback;
}

godot::String AudioStreamAWB::_get_stream_name() const {
	return (source_stream.is_valid() ? source_stream->source_path : source_path).get_file();
}
