{ nixpkgs ? import <nixpkgs> {} }:
let
  inherit (nixpkgs) pkgs;
  packages = [ ];
  myTexLive = with pkgs; texlive.combine { inherit (texlive) scheme-full; };
in pkgs.stdenv.mkDerivation {
  name = "dm1-latex";
  buildInputs = with pkgs; [ less man myTexLive gnumake ] ++ packages;
}

