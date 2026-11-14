#pragma once

#include "godot_cpp/classes/audio_stream.hpp"
#include "godot_cpp/classes/audio_stream_playback_resampled.hpp"
#include "godot_cpp/variant/string.hpp"

extern "C" {
#include "thirdparty/vgmstream/src/libvgmstream.h"
}

#include <vector>

class AudioStreamAWBPlayback : public godot::AudioStreamPlaybackResampled {
	GDCLASS(AudioStreamAWBPlayback, godot::AudioStreamPlaybackResampled)

	libvgmstream_t *decoder = nullptr;
	std::vector<float> mix_buffer;
	int32_t sample_rate = 0;
	int32_t channels = 0;
	bool playing = false;

protected:
	static void _bind_methods();

public:
	AudioStreamAWBPlayback() = default;
	~AudioStreamAWBPlayback() override;

	bool open_stream(const godot::String &p_source_path, int32_t p_subsong_index);

	void _start(double p_from_pos) override;
	void _stop() override;
	bool _is_playing() const override;
	int32_t _get_loop_count() const override;
	double _get_playback_position() const override;
	void _seek(double p_position) override;
	int32_t _mix_resampled(godot::AudioFrame *p_buffer, int32_t p_frames) override;
	float _get_stream_sampling_rate() const override;
};

class AudioStreamAWB : public godot::AudioStream {
	GDCLASS(AudioStreamAWB, godot::AudioStream)

	godot::String source_path;
	godot::Ref<AudioStreamAWB> source_stream;
	int32_t subsong_index = 1;

	friend class AudioStreamAWBLoader;

protected:
	static void _bind_methods();

public:
	void set_source_stream(const godot::Ref<AudioStreamAWB> &p_source_stream);
	godot::Ref<AudioStreamAWB> get_source_stream() const;

	void set_subsong_index(int32_t p_subsong_index);
	int32_t get_subsong_index() const;

	godot::Ref<godot::AudioStreamPlayback> _instantiate_playback() const override;
	godot::String _get_stream_name() const override;
};
