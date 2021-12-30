{
  description = "A Qt interface to GeniCam ethernet cameras via the Aravis library.";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-21.11";
    flake-utils.url = "github:numtide/flake-utils";
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, flake-utils, flake-compat }: let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
    buildDeps = with pkgs; [
      # aravis and dependencies
      aravis
      libxml2

      # glib and dependencies
      glib
      libuuid
      libselinux
      libsepol

      pcre
      ffmpeg
      opencv
      gst_all_1.gstreamer

      qt5.qtbase
      qt5.qtsvg
      qt5.qttools
      qt5.wrapQtAppsHook
    ];
    buildTools = with pkgs; [
      cmake
      pkg-config
      doxygen
      graphviz
    ];
    shellTools = with pkgs; [
      git
      ninja
      ccls
    ];
  in rec {
    devShell.x86_64-linux = pkgs.mkShell {
      buildInputs = buildDeps ++ buildTools ++ shellTools;

      shellHook = ''
        tmpfile=$(mktemp /tmp/nix-develop-XXXXXXXXXX)
        trap "rm $tmpfile" EXIT
        random=$RANDOM$RANDOM$RANDOM$RANDOM
        makeWrapper "$(type -p sh)" "$tmpfile" "''${qtWrapperArgs[@]}" --argv0 "$random"
        sed "/$random/d" -i "$tmpfile"
        source "$tmpfile"
      '';
    };

    defaultPackage.x86_64-linux = pkgs.stdenv.mkDerivation {
      name = "qarv";
      description = "neki";
      src = self;
      buildInputs = buildDeps;
      nativeBuildInputs = buildTools;
    };

    apps.x86_64-linux.qarv = flake-utils.lib.mkApp {
      drv = defaultPackage.x86_64-linux;
      name = "qarv";
    };

    apps.x86_64-linux.qarv_videoplayer = flake-utils.lib.mkApp {
      drv = defaultPackage.x86_64-linux;
      name = "qarv_videoplayer";
    };

    defaultApp.x86_64-linux = apps.x86_64-linux.qarv;
  };
}
