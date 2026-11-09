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
            version = pkgs.lib.removeSuffix "\n" (builtins.readFile ./VERSION);
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

            # Completions come from the binary itself so they always match
            # the installed commands; the throwaway data dir keeps the build
            # from looking for a home directory.
            postInstall = ''
              export CATALIST_DATA_DIR="$TMPDIR/catalist-data"
              installShellCompletion --cmd catalist \
                --bash <($out/bin/catalist completion bash) \
                --zsh <($out/bin/catalist completion zsh) \
                --fish <($out/bin/catalist completion fish)

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
