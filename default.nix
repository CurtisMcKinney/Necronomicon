with import <nixpkgs> {};

let
  IS_DEBUG = false;
  mkCmakeFlag = optSet: flag: if optSet then "-D${flag}=ON" else "-D${flag}=OFF";
  mkFlag = cond: name: if cond then "--enable-${name}" else "--disable-${name}";
  stdenvCompiler = overrideCC stdenv clang_7;
  # stdenvCompiler = overrideCC stdenv gcc9;
in
  stdenvCompiler.mkDerivation rec {
    name = "necro";
    src = ./.;


    llvm_7 = pkgs.llvm_7.overrideAttrs (attrs: {
        separateDebugInfo = IS_DEBUG;
        });

    buildInputs = [ llvm_7 valgrind bear portaudio libsndfile ];
    propagatedBuildInputs = with pkgs; [ xorg.xlibsWrapper ];
    nativeBuildInputs = [ bear cmake ];
    debugVersion = IS_DEBUG;
    enableDebugging = IS_DEBUG;
    enableDebugInfo = IS_DEBUG;
    enableSharedExecutables = false;
    hardeningDisable = [ "all" ];

    preConfigure = ''
      export LDFLAGS="-lX11 -lportaudio -lsndfile"
      export NIX_CFLAGS_COMPILE="$NIX_CFLAGS_COMPILE -fdebug-prefix-map=/build/Necronomicon=." LD=$CC
      '';

    configureFlags = [
      (mkFlag IS_DEBUG "debug")
      (mkFlag IS_DEBUG "debug-symbols")
    ];

    cmakeBuildType = if IS_DEBUG then "Debug" else "Release";

    installPhase = ''
      mkdir -p $out/build
      cp necro $out/build/
      mkdir -p $out/build/Necronomicon
      cp -r $src/source $out/build/Necronomicon/source
      sed -i "s|/build/Necronomicon/build|$out/build|g" ./compile_commands.json
      sed -i "s|/build/Necronomicon/source|$src/source|g" ./compile_commands.json
      cp ./compile_commands.json $out/compile_commands.json
      '';

    dontStrip = IS_DEBUG;
    dontPatchELF = IS_DEBUG;

    meta = with stdenv.lib; {
      description = "Necronomicon is a pure functional data flow language for music and audio DSP.";
      homepage = https://github.com/CurtisMcKinney/Necronomicon;
      maintainers = with maintainers; [ ChadMcKinney CurtisMcKinney ];
      license = licenses.mit;
      platforms = platforms.linux;
    };
  }
