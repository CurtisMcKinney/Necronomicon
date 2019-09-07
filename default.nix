with import <nixpkgs> {};

let
  mkCmakeFlag = optSet: flag: if optSet then "-D${flag}=ON" else "-D${flag}=OFF";
  mkFlag = cond: name: if cond then "--enable-${name}" else "--disable-${name}";
  stdenvCompiler = overrideCC stdenv clang_7;
  # stdenvCompiler = overrideCC stdenv gcc9;
in
  stdenvCompiler.mkDerivation {
    name = "necro";
    src = ./.;

    buildInputs = [ llvm_7 valgrind bear ];
    nativeBuildInputs = [ bear cmake ];
    debugVersion = true;
    enableDebugging = true;
    enableDebugInfo = true;

    preConfigure = ''
      export NIX_CFLAGS_COMPILE="$NIX_CFLAGS_COMPILE -fdebug-prefix-map=/build/Necronomicon=."
      '';

    configureFlags = [
      (mkFlag true "debug")
      (mkFlag true "debug-symbols")
    ];

    cmakeBuildType = "Debug";

    installPhase = ''
      mkdir -p $out/build
      cp necro $out/build/
      mkdir -p $out/build/Necronomicon
      cp -r $src/source $out/build/Necronomicon/source
      sed -i "s|/build/Necronomicon/build|$out/build|g" ./compile_commands.json
      sed -i "s|/build/Necronomicon/source|$src/source|g" ./compile_commands.json
      cp ./compile_commands.json $out/compile_commands.json
      '';

    dontStrip = true;
    dontPatchELF = true;

    meta = with stdenv.lib; {
      description = "Necronomicon is a pure functional data flow language for music and audio DSP.";
      homepage = https://github.com/CurtisMcKinney/Necronomicon;
      maintainers = with maintainers; [ ChadMcKinney CurtisMcKinney ];
      license = licenses.mit;
      platforms = platforms.linux;
    };
  }
