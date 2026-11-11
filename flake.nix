{
  description = "Catalist - local-first anime and manga tracker for the terminal (C++17 / FTXUI)";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "catalist";
            version = pkgs.lib.removeSuffix "\n" (builtins.readFile ./VERSION);
            src = ./.;

            nativeBuildInputs = with pkgs; [
              cmake
              ninja
              pkg-config
              makeWrapper
            ];

            buildInputs = with pkgs; [
              ftxui
              doctest
            ];

            postInstall = ''
              wrapProgram $out/bin/catalist \
                --prefix PATH : ${
                  pkgs.lib.makeBinPath (
                    with pkgs;
                    [
                      curl
                      libsixel
                    ]
                  )
                }
            '';

            meta = {
              description = "Anime and Manga Watchlist TUI";
              homepage = "https://github.com/McKaigne/Catalist";
              license = pkgs.lib.licenses.mit;
              platforms = pkgs.lib.platforms.unix;
              mainProgram = "catalist";
            };
          };
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              cmake
              gcc
              gdb
              ninja
              pkg-config
              git
              ftxui
              doctest
              curl
              libsixel
              clang-tools
            ];
          };
        }
      );
    };
}
