# NigelCPP

Qt6/QML training app for Nigel — a Rocket League bot using PPO reinforcement learning.

## Prerequisites

- **Visual Studio 2022** with C++ desktop workload
- **Qt 6.x** (install via [Qt Online Installer](https://www.qt.io/download-qt-installer-oss))
  - Select: Qt 6.x > MSVC 2022 64-bit
  - Note the install path (e.g. `C:\Qt\6.8.2\msvc2022_64`)
- **CUDA Toolkit 12.x** (for GPU training): https://developer.nvidia.com/cuda-downloads
- **CMake 3.20+** (included with Visual Studio)

## Setup

### 1. Clone

```
git clone https://github.com/jjwd1/NigelCPP.git
cd NigelCPP
```

### 2. Download LibTorch

Download the **LibTorch C++ CUDA** build matching your CUDA version from https://pytorch.org/get-started/locally/

Extract it into `GigaLearnCPP/libtorch/` so the structure looks like:
```
GigaLearnCPP/
  libtorch/
    bin/
    include/
    lib/
    share/
```

### 3. Configure Qt path

The Qt path is set in `core/CMakeLists.txt` line 20:
```cmake
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
```
Update this to match your Qt install path if different.

### 4. Open in Visual Studio

Open the folder in Visual Studio (File > Open > Folder). VS will auto-detect `CMakeSettings.json` and configure the project.

Select **x64-Release** from the configuration dropdown.

### 5. Build

Build > Build All (Ctrl+Shift+B)

Output: `out/build/x64-Release/core/NigelTrainer.exe`

## Running

Copy `collision_meshes/` to the build output directory if not already there, then run the exe.

The app provides:
- Start/stop/pause training
- Live metrics and reward breakdown
- Editable reward weights (persist across sessions)
- Checkpoint management
- GPU monitoring
- Visualizer (requires RocketSimVis Python setup)
