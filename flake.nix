{
  description = "YAML koka";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    flake-utils.url = "github:numtide/flake-utils";
    koka.url = "github:syaiful6/koka-overlay";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    ...
  } @ inputs: let
    overlays = [
      (final: prev: {
        kokapkgs = inputs.koka.packages.${prev.system};
      })
    ];
    # Our supported systems are the same supported systems as the Koka binaries
    systems = builtins.attrNames inputs.koka.packages;
  in
    flake-utils.lib.eachSystem systems (
      system: let
        pkgs = import nixpkgs { inherit overlays system;};
        kokaVersion = "3.1.3";
        koka = pkgs.kokapkgs.${kokaVersion};
      in rec {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            koka
            pkg-config
            libyaml
          ];
        };

        devShell = self.devShells.${system}.default;
      }
    );
}
