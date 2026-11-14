# Godot vgmstream

This GDExtension adds an `AudioStreamAWB` resource that decodes AWB audio banks
with the vendored [vgmstream](https://github.com/vgmstream/vgmstream) library.

## Build

Initialize the submodules and build with SCons:

```sh
git submodule update --init --recursive
scons
```

The extension is copied into `project/bin/` by the build. Vgmstream's C sources
are compiled into the extension; no extra development packages are required.

## Play an AWB file

AWB files are loaded directly as `AudioStreamAWB` resources. Assign an `.awb`
file to an `AudioStreamPlayer`'s **Stream** property in the Inspector, or load it
in GDScript:

```gdscript
var stream := load("res://audio.awb") as AudioStreamAWB
$AudioStreamPlayer.stream = stream
$AudioStreamPlayer.play()
```

The stream defaults to subsong 1. To choose another subsong, set the
`subsong_index` property; indices are 1-based. To save that choice in a scene,
create an `AudioStreamAWB` resource (for example, `res://audio_subsong.tres`),
assign the `.awb` resource to its **Source Stream** property, and set its
**Subsong Index**. The number of subsongs is not detected in the Inspector, so
an invalid index is reported when playback starts. The file is read through
Godot's `FileAccess` and decoded at runtime. Multichannel audio is downmixed to
stereo.
