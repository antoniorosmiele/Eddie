# Eddie
This project has as its starting point all the work done previously in project of EDDIE, part of the Oniro* Project, whose main git repository can be reached via this link: https://gitlab.eclipse.org/eclipse/oniro-core/eddie .


*\*Oniro is a trademark of Eclipse Foundation.*

# About
The purpose of this new project is to take the existing Eddie middleware for Oniro devices and modify it to improve in flexibility and complexity the selection of services or resources deployed in a network of Linux devices.
In brief the new version of the middleware will be able to:

- running on top of the operating system of the various Linux
devices.

- giving to the applications and services a global view
on a unified pool of virtual resources

- allow the user to compose a request to request specific types of resources
  with constraints on features of these resources through the definition of a DCOP problem. (Distributed constraint optimization) 

- solve the DCOP problem using the MGM algorithm by finding if possible a local optimal selection of services and resources available in the network.
  Such a solution can be exploited for an efficient execution of any application or composite service required by the user.

# 📦 Eddie - Installation and Build from Source

This guide will walk you through installing **Eddie** from source on an Ubuntu-based machine.

---

## ✅ Prerequisites

Make sure you are running a fully updated **Ubuntu** system.

---

##  1. Download and Extract Eddie Archive

Download Eddie from the GitHub repository:

```bash
wget https://github.com/antoniorosmiele/Eddie/archive/refs/heads/main.zip
unzip Eddie-main.zip
cd Eddie-main/
```

> Alternatively, you can clone the repository directly:
> ```bash
> git clone https://github.com/antoniorosmiele/Eddie.git
> cd Eddie/
> ```

---

##  2. Install Software Prerequisites

Install all required dependencies:

```bash
sudo apt update
sudo apt install doxygen graphviz libjsoncpp-dev libgtest-dev git python3-full python3-pip python3-packaging pkg-config cmake -y
```

Install the required Python package:

```bash
pip3 install packaging
```

> ⚠️ Note: Some Ubuntu versions may not include **ninja** by default. If needed during the setup, install it with:
> ```bash
> sudo apt install ninja-build
> ```

---

##  3. Install CoAP Library

Install required tools and build **libcoap**:

```bash
sudo apt-get install autoconf libtool -y
git clone https://github.com/obgm/libcoap.git
cd libcoap
git checkout release-4.3.0
./autogen.sh
./configure --disable-documentation --disable-dtls
make
sudo make install
sudo ldconfig
cd ..
```

---

## 4. Install Meson

You can install **Meson** in two ways:

### Recommended (via pip):
```bash
pip3 install --user meson
```

### Alternative (via apt):
```bash
sudo apt install python3-mesonpy
sudo pip3 install meson
```

---

## 5. Install GLib

Clone and build the **GLib** library:

```bash
git clone https://github.com/GNOME/glib
cd glib/
git submodule update --init --recursive
meson setup _build
meson compile -C _build
sudo meson install -C _build
cd ..
```

---

## 6. Build Eddie

Now you can build Eddie:

```bash
cd Eddie/  # or Eddie-main/ if you used the ZIP archive
mkdir build && cd build
cmake ..
make
sudo make install
```

---

## 🎉 Done!

Eddie is now installed on your system. You can execute it or include it in your own projects.

---

## References

- [Eddie GitHub Repo](https://github.com/antoniorosmiele/Eddie)
- [libcoap](https://github.com/obgm/libcoap)
- [GLib](https://github.com/GNOME/glib)

---

## Contributing

See the `CONTRIBUTING.md` file.

## License

See the `LICENSES` subdirectory.

## Security

See the `SECURITY.md` file.
