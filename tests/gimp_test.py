# Executed by tests/run-gimp-tests.sh inside GIMP's python-fu-eval.
import os
import struct

import gi

gi.require_version("Gimp", "3.0")
gi.require_version("Gegl", "0.4")
from gi.repository import Gegl, Gimp, Gio

tmp = os.environ["GIMP_TEST_DIR"]
pdb = Gimp.get_pdb()
failures = []

PAA_LOAD = "plug-in-arma-tools-agp-paa-load"
PAA_EXPORT = "plug-in-arma-tools-agp-paa-export"
EDDS_LOAD = "plug-in-arma-tools-agp-edds-load"


def check(name, condition, detail=""):
    print(("PASS " if condition else "FAIL ") + name + (f": {detail}" if detail else ""))
    if not condition:
        failures.append(name)


def gfile(name):
    return Gio.File.new_for_path(os.path.join(tmp, name))


def run(name, **args):
    proc = pdb.lookup_procedure(name)
    config = proc.create_config()
    for key, value in args.items():
        config.set_property(key.replace("_", "-"), value)
    result = proc.run(config)
    return [result.index(i) for i in range(result.length())]


def new_image(width, height, base=Gimp.ImageBaseType.RGB, layer_type=Gimp.ImageType.RGBA_IMAGE):
    image = Gimp.Image.new(width, height, base)
    layer = Gimp.Layer.new(image, "bg", width, height, layer_type, 100, Gimp.LayerMode.NORMAL)
    image.insert_layer(layer, None, 0)
    return image, layer


def fill(drawable, color):
    Gimp.context_set_foreground(Gegl.Color.new(color))
    drawable.fill(Gimp.FillType.FOREGROUND)


def set_pixels(layer, data):
    buffer = layer.get_buffer()
    rect = Gegl.Rectangle.new(0, 0, layer.get_width(), layer.get_height())
    buffer.set(rect, "R'G'B'A u8", data)
    buffer.flush()
    layer.update(0, 0, layer.get_width(), layer.get_height())


def pixels(image, encoding="R'G'B'A u8"):
    layer = image.get_layers()[0]
    rect = Gegl.Rectangle.new(0, 0, image.get_width(), image.get_height())
    return bytes(layer.get_buffer().get(rect, 1.0, encoding, Gegl.AbyssPolicy.NONE))


def export(image, name):
    return run(PAA_EXPORT, run_mode=Gimp.RunMode.NONINTERACTIVE, image=image, file=gfile(name))


def load(proc, name):
    return run(proc, run_mode=Gimp.RunMode.NONINTERACTIVE, file=gfile(name))


for name in [PAA_LOAD, PAA_EXPORT, EDDS_LOAD]:
    check(f"registered {name}", pdb.procedure_exists(name))

# PAA round trip, a flat color with alpha survives DXT5 compression exactly.
image, layer = new_image(64, 32)
set_pixels(layer, bytes([255, 0, 0, 128]) * (64 * 32))
result = export(image, "red.paa")
check("export rgba", result[0] == Gimp.PDBStatusType.SUCCESS, result)
result = load(PAA_LOAD, "red.paa")
check("load rgba", result[0] == Gimp.PDBStatusType.SUCCESS, result[:2])
loaded = result[1]
check("load rgba size", (loaded.get_width(), loaded.get_height()) == (64, 32))
check("load rgba layer name", loaded.get_layers()[0].get_name() == "red.paa")
px = pixels(loaded)
check("load rgba pixels", px[:4] == bytes([255, 0, 0, 128]) and px == px[:4] * (64 * 32), list(px[:8]))

# File type detection through the registered extension/magics.
loaded = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, gfile("red.paa"))
check("gimp-file-load picks the PAA loader", loaded is not None and loaded.get_width() == 64)

# RGB without alpha is exported opaque.
image, layer = new_image(16, 16, layer_type=Gimp.ImageType.RGB_IMAGE)
fill(layer, "rgb(0, 0, 255)")
check("export rgb", export(image, "blue.paa")[0] == Gimp.PDBStatusType.SUCCESS)
px = pixels(load(PAA_LOAD, "blue.paa")[1])
check("rgb exported opaque", px == bytes([0, 0, 255, 255]) * 256, list(px[:8]))

# Grayscale images are converted to RGB by the export options.
image, layer = new_image(16, 16, Gimp.ImageBaseType.GRAY, Gimp.ImageType.GRAY_IMAGE)
fill(layer, "white")
result = export(image, "gray.paa")
check("export gray", result[0] == Gimp.PDBStatusType.SUCCESS, result)
check("gray exported as white", pixels(load(PAA_LOAD, "gray.paa")[1]) == bytes([255] * 4) * 256)
check("original image untouched", image.get_base_type() == Gimp.ImageBaseType.GRAY and len(Gimp.get_images()) > 0)

# Visible layers are merged; the top layer covers only the left half.
image, bottom = new_image(16, 16)
fill(bottom, "rgb(0, 255, 0)")
top = Gimp.Layer.new(image, "top", 8, 16, Gimp.ImageType.RGBA_IMAGE, 100, Gimp.LayerMode.NORMAL)
image.insert_layer(top, None, 0)
fill(top, "rgb(255, 255, 255)")
check("export layers", export(image, "layers.paa")[0] == Gimp.PDBStatusType.SUCCESS)
px = pixels(load(PAA_LOAD, "layers.paa")[1])
check("layers merged", px[:4] == bytes([255, 255, 255, 255]) and px[-4:] == bytes([0, 255, 0, 255]),
      (list(px[:4]), list(px[-4:])))
check("export does not leak images", len(image.get_layers()) == 2)

# Dimensions have to be a power of two.
image, layer = new_image(30, 32)
result = export(image, "npot.paa")
check("npot status", result[0] == Gimp.PDBStatusType.CALLING_ERROR, result)
check("npot message", "power of two" in (result[1] or ""), result[1])
check("npot nothing written", not os.path.exists(os.path.join(tmp, "npot.paa")))

# Invalid files report errors.
open(os.path.join(tmp, "broken.paa"), "wb").write(b"\x05\xff" + b"\x00" * 3)
result = load(PAA_LOAD, "broken.paa")
check("broken paa status", result[0] == Gimp.PDBStatusType.EXECUTION_ERROR, result)
check("broken paa message", "Import" in (result[1] or ""), result[1])

# EDDS: an uncompressed A8R8G8B8 texture with a single COPY mipmap.
width, height = 4, 2
bgra = bytes([10, 20, 30, 40]) * (width * height)  # stored as B, G, R, A
pixel_format = struct.pack("<8I", 32, 0x41, 0, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000)
header = struct.pack("<7I", 124, 0x2100F, height, width, width * 4, 0, 1) + b"\x00" * 44
header += pixel_format + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
edds = b"DDS " + header + b"COPY" + struct.pack("<I", len(bgra)) + bgra
open(os.path.join(tmp, "test.edds"), "wb").write(edds)
result = load(EDDS_LOAD, "test.edds")
check("load edds", result[0] == Gimp.PDBStatusType.SUCCESS, result[:2])
if result[0] == Gimp.PDBStatusType.SUCCESS:
    loaded = result[1]
    check("load edds size", (loaded.get_width(), loaded.get_height()) == (width, height))
    px = pixels(loaded)
    check("load edds pixels", px == bytes([30, 20, 10, 40]) * (width * height), list(px[:8]))

open(os.path.join(tmp, "broken.edds"), "wb").write(b"DDS " + b"\x00" * 20)
result = load(EDDS_LOAD, "broken.edds")
check("broken edds status", result[0] == Gimp.PDBStatusType.EXECUTION_ERROR, result)

print("FAIL some" if failures else "PASS all")
