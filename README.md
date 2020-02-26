# Paragon Firewall for Mac Community Edition
While built-in macOS firewall efficiently blocks unwanted incoming connections, Paragon Firewall for Mac helps you control programs and services that send information to the Internet, making sure not a single bit of data leaves your computer without your permission.

Project page: https://www.paragon-software.com/home/firewall-mac

## Changelog

|    Date     | Event |
|-------------|-------|
| 2019        | Create |
| February, 2020 | Publication |

## Key Features

* **Silent mode:** Sometimes, it’s convenient to work in silence. You can turn off all notifications and temporarily allow or block all outgoing connections.
* **Alert mode:** Whenever a new app tries to go online, you’re prompted whether you want to allow it to communicate over the Internet, and your choice is remembered.
* **Block separate app:** Block single app allowing others to work freely. Blocked apps won't be able to set outgoing connections with remote servers and to send data.
* **Activate/deactive through one click:** To stop the app you need to click a button and all connections will be recovered. Your settings and rules will be preserved.


## Build instructions:
[> **Firewall - Network Monitor** <](https://apps.apple.com/app/apple-store/id1477597795?pt=470524&ct=Github&mt=8)

1. Open FirewallApp Xcode Project
2. Set `DEVELOPMENT_TEAM` and `ORG_IDENTIFIER` in Config.xcconfig file
3. Build and run FirewallApp scheme

## Limitations
1. macOS Catalina only
2. Apple Developer account

## Feedback
If you have any questions, concerns, or other feedback, please let us know on Github issues or contact us through:
https://www.paragon-software.com/home/firewall-mac/#contact

## Licensing
This project is licensed under the GPL License - see the [LICENSE.md](LICENSE.md) file for details.