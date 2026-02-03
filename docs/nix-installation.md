# Nix/NixOS Installation

Build from source using flake:

```bash
git clone https://github.com/Nomadcxx/gSlapper.git
cd gSlapper
nix build
./result/bin/gslapper --help
```

For development:

```bash
nix develop
```

## System Installation

Add to your NixOS configuration:

```nix
{ config, pkgs, ... }:

{
  environment.systemPackages = with pkgs; [
    gslapper
  ];
}
```

Or using flake directly:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    gslapper.url = "github:Nomadcxx/gSlapper";
  };

  outputs = { self, nixpkgs, gslapper }: {
    nixosConfigurations.default = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ({ pkgs, ... }: {
          environment.systemPackages = [ gslapper.packages.${pkgs.system}.gslapper ];
        })
      ];
    };
  };
}
```

## Home Manager

Add to your Home Manager configuration:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
    gslapper.url = "github:Nomadcxx/gSlapper";
  };

  outputs = { self, nixpkgs, gslapper }: {
    homeConfigurations.default = {
      pkgs = nixpkgs.legacyPackages.${system};
      home.packages = [ gslapper.packages.${system}.gslapper ];
    };
  };
}
```