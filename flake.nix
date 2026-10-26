{
  description = "Catalist - Anime and Manga Watchlist TUI (C++17 / FTXUI)";

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
            version = "0.2.0";
            src = ./.;

            nativeBuildInputs = with pkgs; [
              cmake
              ninja
              pkg-config
              makeWrapper
            ];

            buildInputs = with pkgs; [
              ftxui
            ];

            postInstall = ''
              wrapProgram $out/bin/catalist \
                --prefix PATH : ${pkgs.lib.makeBinPath (with pkgs; [
                  curl
                  wl-clipboard
                  xclip
                  xdg-utils
                ])}
            '';

            meta = {
              description = "Anime and Manga Watchlist TUI";
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
              curl
              wl-clipboard
              xclip
              xdg-utils
            ];
          };
        }
      );
    };
}
