
Download eddie's archive from the link: https://gitlab.eclipse.org/eclipse/oniro-core/eddie
and then extract the archive

Software prerequisite installation:
cd eddie-main/
sudo apt install doxygen
sudo apt install graphviz
sudo apt install libjsoncpp-dev
sudo apt install libgtest-dev
sudo apt install git
sudo apt install python3-full
sudo apt install python3-pip
pip3 install packaging (sudo apt install python3-packaging)
sudo apt install pkg-config
sudo apt install cmake

Some versions of ubuntu may lack ninja

Install Coap:
sudo apt-get install autoconf -y
sudo apt install libtool -y
git clone https://github.com/obgm/libcoap.git
cd libcoap
git checkout release-4.3.0
./autogen.sh
./configure --disable-documentation --disable-dtls
make
sudo make install
sudo ldconfig

Install meson:
(sudo apt install python3-mesonpy)
pip3 install --user meson
sudo pip3 install meson

Install Glib:
cd ../
git clone https://github.com/GNOME/glib
cd glib/
git submodule update --init --recursive
meson setup _build
meson compile -C _build
meson install -C _build

Build Eddie:
cd ../
mkdir build && cd build
cmake ..
make
sudo make install


