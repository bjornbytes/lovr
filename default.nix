{ config ? {}, lib ? {}, pkgs ? import <nixpkgs> {}, ... }:

pkgs.stdenv.mkDerivation {
  name = "lovr";
  src = ./.; /*pkgs.fetchFromGitHub {
    owner = "bjornbytes";
    repo = "lovr";
    sha256 = "0gqp8w68zk9sjp10nl2drs5qwnjx1qv4yqrrr2ny0z9vx2v531l4";
    rev = "7ca23bc58a065c4be05af0a2717ee83bc501ecee";
    fetchSubmodules = true;
  };*/

  nativeBuildInputs = [
    pkgs.cmake
    pkgs.libglvnd.dev
    pkgs.pkgconfig
    pkgs.x11
    pkgs.xorg.libXcursor
    pkgs.libxkbcommon
    pkgs.xorg.libXinerama
    pkgs.xorg.libXrandr
    pkgs.xorg.libXi
 ];
  
  configurePhase = ''cmake -B build -D OpenGL_GL_PREFERENCE=GLVND -D CMAKE_INSTALL_PREFIX=$out .'';
  buildPhase = ''cmake --build build --parallel $NIX_BUILD_CORES; ls build  '';
  installPhase = ''cmake --install build
  patchelf --print-rpath build/lovr
  cp build/lovr $out/bin/lovr'';
}
