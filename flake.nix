{
  description = "Free42";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      systems = lib.platforms.unix;
      forAllSystems = f: lib.genAttrs systems (system: f system);
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          src = ./.;
        in
        {
          default = self.packages.${system}.gtk;

          gtk = pkgs.stdenv.mkDerivation {
            pname = "free42-gtk";
            version = "unstable";
            inherit src;

            dontConfigure = true;

            nativeBuildInputs =
              with pkgs;
              [
                pkg-config
                gtk3
                which
                makeWrapper
              ]
              ++ lib.optional stdenv.isDarwin cctools;

            buildPhase = ''
              runHook preBuild
              patchShebangs build-gtk gtk/build-intel-lib.sh
              ./build-gtk
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall

              pkgName="Free42$(uname -s)"
              archive="packages/$pkgName.tgz"

              mkdir -p "$out/packages" "$out/share" "$out/bin"
              cp -v "$archive" "$out/packages/"
              tar -xzf "$archive" -C "$out/share"

              wrapperArgs=()
              if [ "$(uname -s)" = "Darwin" ]; then
                wrapperArgs+=(--unset DISPLAY)
                wrapperArgs+=(--set-default GDK_BACKEND quartz)
              fi

              makeWrapper "$out/share/$pkgName/free42bin" "$out/bin/free42bin" "''${wrapperArgs[@]}"
              makeWrapper "$out/share/$pkgName/free42dec" "$out/bin/free42dec" "''${wrapperArgs[@]}"

              runHook postInstall
            '';

            meta = with pkgs.lib; {
              description = "Free42 GTK package built via build-gtk";
              homepage = "https://thomasokken.com/free42/";
              license = licenses.gpl2Only;
              platforms = systems;
              mainProgram = "free42bin";
            };
          };
        }
      );
    };
}
