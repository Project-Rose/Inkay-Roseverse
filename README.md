
# Inkay (Roséverse)
<img width="1000" height="250" alt="Inkay (Roséverse) by Project Rosé (README banner)" src="https://github.com/user-attachments/assets/be3959c7-58ce-4d88-87f7-db03bb4cf9a0" />

[![GitHub release](https://img.shields.io/github/release/Project-Rose/Inkay-Roseverse.svg)](https://github.com/Project-Rose/Inkay-Roseverse/releases) [![Github all releases](https://img.shields.io/github/downloads/Project-Rose/Inkay-Roseverse/total.svg)](https://gitHub.com/Project-Rose/Inkay-Roseverse/releases) [![GitHub License](https://img.shields.io/github/license/Project-Rose/Inkay-Roseverse)](https://github.com/Project-Rose/Inkay-Roseverse/blob/master/LICENSE) [![Discord server](https://img.shields.io/discord/1277130014357327873?color=5865f2&label=Discord)](https://discord.gg/AaTsXndGun) [![Services Health](https://projectrose.montastic.io/badge)](https://projectrose.montastic.io) 

This version of [Inkay](https://github.com/PretendoNetwork/Inkay) lets you connect to [Roséverse](https://miiverse.projectrose.cafe/), Project Rose's Miiverse revival, instead of Pretendo's [Juxtaposition (Juxt)](https://juxt.pretendo.network), while keeping connections to other Pretendo services intact. Read more about Inkay in the [original repository](https://github.com/PretendoNetwork/Inkay).

> [!NOTE]
> This patcher is for the Wii U, not 3DS.

> [!IMPORTANT]
> This plugin will **REPLACE** your existing Inkay plugin. If you want to switch back to Juxt, or if you're having any issues with the plugin, you can always reinstall the original Inkay version from the [Aroma Updater](https://github.com/wiiu-env/AromaUpdater) app on your Wii U console. Please read more details below.

# Getting Started
## Requirements
Inkay (Roséverse) requires a homebrewed Wii U running the latest version of the [Aroma](https://github.com/wiiu-env/Aroma) environment, configured for coldboot (autoboot). Other configurations (specifically lacking coldboot) may cause issues with SpotPass connections to Pretendo's servers. Older homebrew environments (such as Tirumissu, Haxchi, etc.) and hackless methods aren't supported. If you haven't already, homebrew your Wii U using [this guide](https://wiiu.hacks.guide/)!

> [!NOTE]
> Inkay (Roséverse) is currently unavailable on the [HB App Store](https://hb-app.store) due to complications with their package management system. We are actively working to find ways to put Inkay (Roséverse) onto the HB App Store, and the link will be posted here once it is available.

## Installation
This installation process requires a computer or any other device that can read SD cards. You can alternatively use FTP using the [FTPiiU plugin](https://github.com/wiiu-env/ftpiiu_plugin) and [FileZilla](https://filezilla-project.org). The process below only covers the computer method. Please follow these steps very closely:

1. Turn off your Wii U and eject the SD card. Insert the SD card into your device.
1. Download the [latest release](https://github.com/Project-Rose/Inkay-Roseverse/releases) of both `Inkay-pretendo.wms` and `Inkay-pretendo.wps`.
2. Place `Inkay-pretendo.wms` (**WMS**) in the Aroma modules folder on your SD card (usually `sd:/wiiu/environments/aroma/modules`). If it asks you to replace the existing file, replace it.
3. Place `Inkay-pretendo.wps` (**WPS**) in the Aroma plugins folder on your SD card (usually `sd:/wiiu/environments/aroma/plugins`). If it asks you to replace the existing file, replace it.
4. Eject the SD card from your device, and insert the SD card into your Wii U and turn it on. 

An Aroma notification should pop up stating "**Using Pretendo Network (Roséverse)**" once it is successfully installed. Then you can click the Miiverse icon to access Roséverse. If it doesn't have "**(Roséverse)**" in the notification, it isn't installed correctly.

# Configuration
Hold L + Down + Start to open the Wii U Plugin Menu, then scroll down and click Inkay (Roséverse). From here, there are several things that you can configure:
- Network selection > **Connect to Pretendo Network** - Connect to Pretendo Network and Roséverse instead of the Nintendo Network.
- Other settings > **Reset WaraWara Plaza (for Roséverse)** - Force refresh the WaraWara Plaza and pull new WaraWara Plaza data from Project Rosé servers. This is useful if the Juxtaposition plaza still appears after the first installation.

An Aroma notification should tell you if the patch is enabled every time you boot up your console.

# Credits
- [Pretendo Network](https://pretendo.network): For the original [Inkay](https://github.com/PretendoNetwork/Inkay) and coordaniting support, among other things.
- All the people who helped worked on this project, you can see them at https://projectrose.cafe; also, any others who are not mentioned.

# Issues?
> [!NOTE]
If you find any issues with **the patcher itself** or have any suggestions, please open a GitHub Issue [here](https://github.com/Project-Rose/Inkay-Roseverse/issues). If you need support for Roséverse itself or have any questions that aren't answered here, please ask in our [Discord server](https://discord.gg/AaTsXndGu), which is more active.
