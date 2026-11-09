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
              installShellFiles
            ];

            buildInputs = with pkgs; [
              ftxui
              doctest
            ];

            postInstall = ''
              if [ -f doc/catalist.1 ]; then
                installManPage doc/catalist.1
              fi
              if [ -f completions/catalist.bash ]; then
                installShellCompletion --bash completions/catalist.bash
              fi
              if [ -f completions/_catalist ]; then
                installShellCompletion --zsh completions/_catalist
              fi
              if [ -f completions/catalist.fish ]; then
                installShellCompletion --fish completions/catalist.fish
              fi

              wrapProgram $out/bin/catalist \
                --prefix PATH : ${
                  pkgs.lib.makeBinPath (
                    with pkgs;
                    [
                      curl
                      wl-clipboard
                      xclip
                      xdg-utils
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
              wl-clipboard
              xclip
              xdg-utils
              clang-tools
            ];
          };
        }
      );
    };
}
