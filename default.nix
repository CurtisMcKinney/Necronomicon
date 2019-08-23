with import <nixpkgs> {};

let
  stdenvCompiler = overrideCC stdenv clang_7;
  # stdenvCompiler = overrideCC stdenv gcc9;
in
  stdenvCompiler.mkDerivation {
    name = "necro";
    src = ./.;

    buildInputs = [ llvm_7 valgrind ];
    nativeBuildInputs = [ cmake ];
    enableDebugging = true;

    installPhase = ''
      mkdir -p $out/build
      cp necro $out/build/
      '';

    meta = with stdenv.lib; {
      description = "Necronomicon is a pure functional data flow language for music and audio DSP.";
      homepage = https://github.com/CurtisMcKinney/Necronomicon;
      maintainers = with maintainers; [ ChadMcKinney CurtisMcKinney ];
      license = licenses.mit;
      platforms = platforms.linux;
    };
  }
