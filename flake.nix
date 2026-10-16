{
  description = "Free42";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    { self, nixpkgs }:
    {
      packages = nixpkgs.lib.genAttrs nixpkgs.lib.platforms.unix (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          inherit (pkgs) lib;

          packages = self.packages.${system};
          version = "unstable";

          mkSource =
            filesets:
            lib.fileset.toSource {
              root = ./.;
              fileset = lib.fileset.unions filesets;
            };
        in
        {
          default = self.packages.${system}.gtk;

          intel-lib =
            pkgs.runCommandCC "free42-intel-lib-${version}"
              {
                src = mkSource [
                  ./inteldecimal/IntelRDFPMathLib20U1.tar.gz
                  ./gtk/build-intel-lib.sh
                  ./gtk/intel-lib-linux.patch
                  ./gtk/intel-lib-unknown-32bit.patch
                  ./gtk/intel-lib-unknown-64bit.patch
                ];
                sourceRoot = "source/gtk";
                nativeBuildInputs = with pkgs; [ which ] ++ lib.optional stdenv.buildPlatform.isDarwin cctools;
              }
              ''
                runPhase unpackPhase
                MK=make sh ./build-intel-lib.sh
                install -Dm644 gcc111libbid.a "$out/lib/gcc111libbid.a"
                runPhase fixupPhase
              '';

          gtk = pkgs.stdenv.mkDerivation {
            pname = "free42-gtk";
            inherit version;
            src = mkSource [
              ./gtk
              ./common
              ./skins
              ./windows/VERSION.h
            ];
            sourceRoot = "source/gtk";

            dontConfigure = true;
            # Darwin does not expose sincos, so use the separate sin/cos calls.
            env.NIX_CFLAGS_COMPILE = lib.concatStringsSep " " (
              [ "-g0" ] ++ lib.optional pkgs.stdenv.hostPlatform.isDarwin "-UHAVE_SINCOS"
            );
            enableParallelBuilding = true;

            nativeBuildInputs = with pkgs; [
              pkg-config
              glib
              wrapGAppsHook3
            ];

            buildInputs = with pkgs; [ gtk3 ];

            # Leave AUDIO_ALSA unset so all platforms use GTK's default beep backend.
            makeFlags = [ "BCD_MATH=1" ];

            preBuild = ''
              make cleaner symlinks
              cp -fv ${packages.intel-lib}/lib/gcc111libbid.a .
            '';

            installPhase = ''
              runHook preInstall

              mkdir -p "$out/bin" "$out/share/doc/free42" "$out/share/icons/hicolor/128x128/apps"
              install free42dec "$out/bin/free42dec"
              cp README.txt "$out/share/doc/free42/README.txt"
              cp icon-128x128.png "$out/share/icons/hicolor/128x128/apps/free42.png"

              runHook postInstall
            '';

            preFixup = lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
              gappsWrapperArgs+=(--unset DISPLAY --set-default GDK_BACKEND quartz)
            '';

            meta = with lib; {
              description = "Free42 GTK calculator";
              homepage = "https://thomasokken.com/free42/";
              license = licenses.gpl2Only;
              platforms = platforms.unix;
              mainProgram = "free42dec";
            };
          };
        }
      );
    };
}
