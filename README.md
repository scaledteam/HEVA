# HEVA
HEVA - High Efficiency VTuber Avatar. Virtual avatar application, based on Urho3D and DLib.

Additional information can be found in this streams: 

* https://www.youtube.com/watch?v=ZNmIW92sFw8
* https://www.youtube.com/watch?v=J8JZ56pLK_A
* https://www.youtube.com/watch?v=gs9WXCEpbuY

## Usage
For windows, just download latest release and unpack. Launch `heva.exe` . To configure HEVA, edit `heva.ini` file.

https://github.com/scaledteam/HEVA/releases/

To make you own model, you need this Blender Plugin: https://github.com/urho3d-tools/blender-exporter/

Also, repository contain sources (.blend file) of default character, which you can use any way you like: https://github.com/scaledteam/HEVA/blob/main/BlenderModels/Default%20Character.blend

## Compiling for GNU/Linux
Firstly you need to install DLib, OpenCV and Urho3D.

1. Urho3D: https://github.com/urho3d/Urho3D (instructions below)
2. OpenCV: https://github.com/opencv/opencv , or `libopencv-dev libopenblas-dev` if you use Debian.
3. DLib: https://github.com/davisking/dlib , or `libdlib-dev libdlib-data` if you use Debian.
4. IniH: https://github.com/benhoyt/inih , or `libinih-dev` if you use Debian.

```sh
git clone https://github.com/urho3d/Urho3D
cd Urho3D
cmake . \
	-DURHO3D_TOOLS=NO \
	-DURHO3D_ANGELSCRIPT=NO \
	-DURHO3D_LUA=NO \
	-DURHO3D_NETWORK=NO \
	-DURHO3D_SAMPLES=NO \
	-DURHO3D_WEBP=NO \
	-DURHO3D_IK=NO \
	-DURHO3D_THREADING=NO \
	-DURHO3D_NAVIGATION=NO \
	-DURHO3D_URHO2D=NO \
	-DURHO3D_LUAJIT=NO \
	-DURHO3D_LUAJIT_AMALG=NO
make -j$(nproc)
cd ..
git clone https://github.com/scaledteam/HEVA
mkdir build
cd build
# Change path to your own location
cmake .. -DX11_TRANSPARENT_WINDOW=YES -DURHO3D_HOME=/home/$USER/Urho3D
make -j$(nproc)
```

## Cross compiling from GNU/Linux for Windows

Clone this 2 repositories somewhere

1. Urho3D: https://github.com/urho3d/Urho3D
2. OpenCV: https://github.com/opencv/opencv

Clone this 2 repositories into HEVA folder

1. https://github.com/davisking/dlib
2. https://github.com/benhoyt/inih

Build Urho3D
```sh
cmake . \
	-DCMAKE_SYSTEM_NAME=Windows \
	-DCMAKE_CXX_COMPILER=/usr/bin/x86_64-w64-mingw32-g++ \
	-DCMAKE_C_COMPILER=/usr/bin/x86_64-w64-mingw32-gcc   \
	-DURHO3D_DEPLOYMENT_TARGET=generic \
	-DURHO3D_TOOLS=NO \
	-DURHO3D_ANGELSCRIPT=NO \
	-DURHO3D_LUA=NO \
	-DURHO3D_NETWORK=NO \
	-DURHO3D_SAMPLES=NO \
	-DURHO3D_WEBP=NO \
	-DURHO3D_IK=NO \
	-DURHO3D_THREADING=NO \
	-DURHO3D_NAVIGATION=NO \
	-DURHO3D_URHO2D=NO \
	-DURHO3D_LUAJIT=NO \
	-DURHO3D_LUAJIT_AMALG=NO \
	-DWIN32=1 \
	-DURHO3D_D3D11=NO
```

Build OpenCV
```sh
cmake .. \
	-DCMAKE_SYSTEM_NAME=Windows \
	-DCMAKE_CXX_COMPILER=/usr/bin/x86_64-w64-mingw32-g++-posix \
	-DCMAKE_C_COMPILER=/usr/bin/x86_64-w64-mingw32-gcc-posix \
	-DWITH_OPENCLAMDBLAS=NO \
	-DWITH_LAPACK=NO \
	-DWITH_FFMPEG=NO \
	-DWITH_LAPACK=NO \
	-DWITH_FFMPEG=NO \
	-DWITH_JPEG=NO \
	-DWITH_IMGCODEC_HDR=NO \
	-DWITH_OPENEXR=NO \
	-DWITH_WIN32UI=NO \
	-DWITH_IMGCODEC_SUNRASTER=NO \
	-DWITH_IMGCODEC_PXM=NO \
	-DWITH_IMGCODEC_PFM=NO \
	-DWITH_OPENCL=NO \
	-DOPENCV_DNN_OPENCL=NO \
	-DBUILD_SHARED_LIBS=NO
```

Build HEVA, change paths at the end to locations where you built Urho3D and OpenCV.

```sh
cmake ..  \
	-DCMAKE_SYSTEM_NAME=Windows \
	-DCMAKE_CXX_COMPILER=/usr/bin/x86_64-w64-mingw32-g++-posix \
	-DCMAKE_C_COMPILER=/usr/bin/x86_64-w64-mingw32-gcc-posix  \
	-DDLIB_JPEG_SUPPORT=NO \
	-DDLIB_GIF_SUPPORT=NO \
	-DDLIB_PNG_SUPPORT=NO \
	-DDLIB_NO_GUI_SUPPORT=YES \
	-DDLIB_USE_LAPACK=NO \
	-DDLIB_USE_CUDA=NO \
	-DURHO3D_DEPLOYMENT_TARGET=generic  \
	-DURHO3D_DEPLOYMENT_TARGET=generic \
	-DALL_IN_ONE=YES \
	-DVMC_OSC_SENDER=NO \
	-DOpenCV_DIR=/home/scaled/projects/Urho3D/HEVA-Windows/opencv/build \
	-DURHO3D_HOME=/home/scaled/projects/Urho3D/HEVA-Windows/Urho3D
```
