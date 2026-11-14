#!/usr/bin/env python
import os
import sys

from methods import print_error


libname = "godot-vgmstream"
projectdir = "project"

localEnv = Environment(tools=["default"], PLATFORM="")

# Build profiles can be used to decrease compile times.
# You can either specify "disabled_classes", OR
# explicitly specify "enabled_classes" which disables all other classes.
# Modify the example file as needed and uncomment the line below or
# manually specify the build_profile parameter when running SCons.

# localEnv["build_profile"] = "build_profile.json"

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"api_version": 4.3, "env": env, "customs": customs})

vgmstream_dir = "src/thirdparty/vgmstream"
env.Append(
    CPPPATH=[
        "src/",
        vgmstream_dir + "/src",
        vgmstream_dir + "/ext_includes",
    ],
    CPPDEFINES=[
        "VGM_USE_G7221",
        "VGM_USE_G719",
        "VGM_USE_ATRAC9",
        "VGM_LOG_OUTPUT",
    ],
)
sources = (
    Glob("src/*.cpp")
    + Glob(vgmstream_dir + "/src/*.c")
    + Glob(vgmstream_dir + "/src/**/*.c")
    + Glob(vgmstream_dir + "/src/coding/libs/*.c")
)

if env["PLATFORM"] != "win32":
    env.Append(LIBS=["m"])

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.extend(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# .dev doesn't inhibit compatibility, so we don't need to key it.
# .universal just means "compatible with all relevant arches" so we don't need to key it.
suffix = env['suffix'].replace(".dev", "").replace(".universal", "")

lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

object_root = os.path.join("build", env["platform"], env["target"], "obj")
objects = []
for source in sources:
    source_path = str(source)
    object_path = os.path.join(
        object_root,
        os.path.splitext(source_path)[0] + env["SHOBJSUFFIX"],
    )
    os.makedirs(os.path.dirname(object_path), exist_ok=True)
    objects.extend(env.SharedObject(target=object_path, source=source))

library = env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=objects,
)

copy = env.Install("{}/bin/{}/".format(projectdir, env["platform"]), library)

default_args = [library, copy]
Default(*default_args)
