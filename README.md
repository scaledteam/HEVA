# HEVA
HEVA - High Efficiency VTuber Avatar. Virtual avatar application, based on Urho3D and DLib.

## Compiling for GNU/Linux
Firstly you need to install DLib, OpenCV and Urho3D.

1. Urho3D: https://github.com/urho3d/Urho3D
2. OpenCV: https://github.com/opencv/opencv , or `opencv-dev` if you use Debian.
3. DLib: https://github.com/davisking/dlib , or `libdlib-dev libdlib-data` if you use Debian.
4. IniH: https://github.com/benhoyt/inih , or `libinih-dev` if you use Debian.

```
git clone https://github.com/scaledteam/HEVA
mkdir build
cd build
cmake .. -DX11_TRANSPARENT_WINDOW=YES
make -j2
```