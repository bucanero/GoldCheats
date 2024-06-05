# PS4 Cheats Manager

[![Downloads][img_downloads]][app_downloads] [![Release][img_latest]][app_latest] [![License][img_license]][app_license]
[![Build package](https://github.com/bucanero/PS4CheatsManager/actions/workflows/build.yml/badge.svg)](https://github.com/bucanero/PS4CheatsManager/actions/workflows/build.yml)

An open-source PlayStation 4 cheat code manager for [GoldHEN](https://github.com/GoldHEN/GoldHEN).

![image](./docs/screenshots/main-menu.jpg)

<details>
<summary>Screenshots Gallery (Click to Expand)</summary>

![image](./docs/screenshots/cheatsmgr-xmb.jpg)
![image](./docs/screenshots/game-list.jpg)
![image](./docs/screenshots/hdd-patch.jpg)

</details>

**PS4 Cheats Manager** is a simple PS4 homebrew application that manages `JSON`, `SHN`, `XML`, and `MC4` cheat codes files.

# Download

Get the [latest version here][app_latest].

<details>
<summary>Nightly Builds (Click to Expand)</summary>

- Login to GitHub [(this is required to download artifacts)](https://docs.github.com/en/actions/managing-workflow-runs/downloading-workflow-artifacts#:~:text=Who%20can%20use,download%20workflow%20artifacts.).
- Click on a build in the **[Actions](../../actions)** Tab.
  - ![image](https://github.com/bucanero/apollo-ps4/assets/37698908/2e52961b-04ef-42f1-9453-78b3ceb24afc)
- Click on the artifact to download the pkg zip.
  - ![image](https://github.com/bucanero/apollo-ps4/assets/37698908/aaeb523e-eab9-43f7-a993-5fa902ada1b7)

</details>

## Changelog

See the [latest changes here](CHANGELOG.md).

# Donations

My GitHub projects are open to a [Patreon sponsor program](https://patreon.com/dparrino). If you feel that my tools helped you in some way or you would like to support future developments, you can consider a [PayPal donation](https://www.paypal.me/bucanerodev).

# Setup instructions

No special setup is needed. Just download the latest `IV0000-CHTM00777_00-PS4CHEATSMANAGER.pkg` package and install it on your PlayStation 4.
On first run, the application will detect and setup the required user settings.

# Usage

Using the application is simple and straight-forward: 

 - Move <kbd>UP</kbd>/<kbd>DOWN</kbd> to select the cheat file you want to view, and press ![X button](https://github.com/bucanero/pkgi-ps3/raw/master/data/CROSS.png). The cheat list screen will show the available codes for the game.
 - To view the item's details, press ![Triangle](https://github.com/bucanero/pkgi-ps3/raw/master/data/TRIANGLE.png).
It will open the context menu on the screen. Press ![O button](https://github.com/bucanero/pkgi-ps3/raw/master/data/CIRCLE.png) to return to the list.
 - To reload the list, press ![Square](https://github.com/bucanero/pkgi-ps3/raw/master/data/SQUARE.png).
 - Press <kbd>L1</kbd>/<kbd>L2</kbd> or <kbd>R1</kbd>/<kbd>R2</kbd> trigger buttons to move pages up or down.

# Online Database

The application also provides direct access to the Online [Cheat](https://github.com/GoldHEN/GoldHEN_Cheat_Repository) and [Patch](https://github.com/illusion0001/PS4-PS5-Game-Patch) databases, the official GoldHEN repositories of cheats and custom game patches for PlayStation 4 games.

# Credits

* [Bucanero](https://github.com/bucanero): Project developer

### GoldHEN credits

* [Ctn123](https://github.com/ctn123): Cheat Engine
* [illusion](https://github.com/illusion0001): Patch Engine
* [Shiningami](https://github.com/ScriptSK): Cheat Engine
* [SiSTRo](https://github.com/SiSTR0): [GoldHEN](https://github.com/GoldHEN/GoldHEN)
* [Kameleon](https://github.com/kmeps4): QA Support

### Artemis PS3 credits

* [Dnawrkshp](https://github.com/Dnawrkshp/): [Artemis PS3](https://github.com/Dnawrkshp/ArtemisPS3)
* [Berion](https://www.psx-place.com/members/berion.1431/): GUI design

# Building

You need to have installed:

- [Open Orbis SDK](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/)
- [cJSON](https://github.com/bucanero/cJSON) library
- [SDL2](https://github.com/PacBrew/SDL/tree/ps4) library
- [libJbc](https://github.com/bucanero/ps4-libjbc) library
- [SQLite](https://github.com/bucanero/libSQLite-ps4) library
- [Mini-XML](https://github.com/bucanero/mxml) library
- [PolarSSL](https://github.com/bucanero/oosdk_libraries/tree/master/polarssl-1.3.9) library
- [cURL](https://github.com/bucanero/oosdk_libraries/tree/master/curl-7.64.1) library
- [dbglogger](https://github.com/bucanero/dbglogger) library

Run `make` to create a release build. If you want to include the latest cheats and patches in your `.pkg` file, run `make createzip`.

You can also set the `PS3LOAD` environment variable to your PS4 IP address: `export PS3LOAD=tcp:x.x.x.x`.
This will allow you to use `make run` and send `eboot.bin` directly to the [PS4Load listener](https://github.com/bucanero/ps4load).

To enable debug logging, build it with `make DEBUGLOG=1`. The application will send debug messages to
UDP multicast address `239.255.0.100:30000`. To receive them you can use [socat][] on your computer:

```
$ socat udp4-recv:30000,ip-add-membership=239.255.0.100:0.0.0.0 -
```

# License

[PS4 Cheats Manager](https://github.com/bucanero/PS4CheatsManager/) - Copyright (C) 2022-2024 [Damian Parrino](https://twitter.com/dparrino)

This program is free software: you can redistribute it and/or modify
it under the terms of the [GNU General Public License][app_license] as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

[app_license]: https://github.com/bucanero/PS4CheatsManager/blob/main/LICENSE
[app_downloads]: https://github.com/bucanero/PS4CheatsManager/releases
[app_latest]: https://github.com/bucanero/PS4CheatsManager/releases/latest
[img_license]: https://img.shields.io/github/license/bucanero/PS4CheatsManager.svg?maxAge=2592000
[img_downloads]: https://img.shields.io/github/downloads/bucanero/PS4CheatsManager/total.svg?maxAge=3600
[img_latest]: https://img.shields.io/github/release/bucanero/PS4CheatsManager.svg?maxAge=3600
[socat]: http://www.dest-unreach.org/socat/
