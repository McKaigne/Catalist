{
  description = "Catalist - local-first anime and manga tracker for the command line (C++23)";

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
              makeWrapper
            ];

            buildInputs = with pkgs; [
              sqlite
            ];

            # The tests are run from the dev shell, not by the package.
            cmakeFlags = [ "-DBUILD_TESTING=OFF" ];

            postInstall = ''
              wrapProgram $out/bin/catalist \
                --prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.curl ]}
            '';

            meta = {
              description = "Anime and manga tracker for the command line";
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
              git
              doctest
              sqlite
              curl
              clang-tools
              # ctest checks that the completion scripts parse.
              zsh
              fish
            ];
          };
        }
      );
    };
}
