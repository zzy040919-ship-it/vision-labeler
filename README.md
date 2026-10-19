# Vision Labeler

A small C++17 desktop tool for camera capture, image browsing, bounding-box annotation, and YOLO/JSON export.

## Requirements

- CMake 3.21+
- Qt 6 Widgets
- OpenCV (core, imgcodecs, videoio)
- A C++17 compiler

## Build

```sh
cmake -S . -B build
cmake --build build --config Release
```

## Use

- **Open images** or **Open folder** to build the image list; use the filter field to find images.
- Choose or type a class, then drag a rectangle on the image. Use the wheel to zoom and right-drag to pan.
- Select a box to rename its class, or delete it with the button/Delete key.
- **Camera** opens camera device 0; **Capture** saves a frame into the image list.
- **Export YOLO** creates `images/`, `labels/`, and `classes.txt`. Each label line uses `class_id x_center y_center width height`, normalized to 0–1.
- **Save JSON** stores image paths and normalized boxes.

Annotations are held in memory until exported. This demo expects unique image filenames when exporting into one dataset folder.
