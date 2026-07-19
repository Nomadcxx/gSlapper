{
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };
  outputs =
    {
      self,
      flake-utils,
      nixpkgs,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = (import nixpkgs) { inherit system; };
      in
      {
        packages = rec {
          gslapper = pkgs.stdenv.mkDerivation {
            src = ./.;
            nativeBuildInputs = with pkgs; [
              wayland-protocols
              egl-wayland
              libglvnd
              gst_all_1.gstreamer
              gst_all_1.gst-plugins-base
              gst_all_1.gst-plugins-bad
              gst_all_1.gst-plugins-good
              systemd
              wayland-scanner
              makeWrapper
            ];
            buildInputs = with pkgs; [
              meson
              pkg-config
              wayland-protocols
              wayland
              egl-wayland
              libglvnd
              gst_all_1.gstreamer
              gst_all_1.gst-plugins-base
              gst_all_1.gst-plugins-bad
              gst_all_1.gst-plugins-good
              systemd
              wayland-scanner
              ninja
            ];
            pname = "gslapper";
            version = "1.5.1";

            postFixup = ''
              wrapProgram $out/bin/gslapper \
                --prefix GST_PLUGIN_SYSTEM_PATH_1_0 : "${
                  pkgs.lib.makeSearchPath "lib/gstreamer-1.0" (
                    with pkgs.gst_all_1;
                    [
                      gstreamer.out
                      gst-plugins-base
                      gst-plugins-good
                      gst-plugins-bad
                    ]
                  )
                }"
            '';
          };
          default = gslapper;
        };
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            wayland-protocols
            egl-wayland
            libglvnd
            gst_all_1.gstreamer
            gst_all_1.gst-plugins-base
            gst_all_1.gst-plugins-bad
            gst_all_1.gst-plugins-good
            systemd
            wayland-scanner
          ];
          buildInputs = with pkgs; [
            meson
            pkg-config
            wayland-protocols
            wayland
            egl-wayland
            libglvnd
            gst_all_1.gstreamer
            gst_all_1.gst-plugins-base
            gst_all_1.gst-plugins-bad
            gst_all_1.gst-plugins-good
            systemd
            wayland-scanner
            ninja
          ];
        };
      }
    );
}
