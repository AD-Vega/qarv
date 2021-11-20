{ pkgs ? import <nixpkgs> {} }: with pkgs; pkgs.mkShell {
  buildInputs = [
    git
    cmake
    ninja
    pkg-config
    pcre
    glib
      libuuid
      libselinux
      libsepol
    doxygen
      graphviz
    aravis
      libxml2
    ffmpeg
    opencv
    gst_all_1.gstreamer
    qt5.full
  ];
}
