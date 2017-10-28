## usplash - a fancy dynamic menu

usplash is a tiny dynamic menu application for the Linux command-line.

It was written specifically for gear0x's splash-screen menu
and is meant as a script-supported init process.

When you run usplash in menu mode (-m) the main-screen will pop up instantly,
now you can add menu items and scripts they should execute.

Opposed to dialog and other menu systems for the command-line, usplash can be changed at runtime;
meaning that background-processes can add and update entries in the menu, or even trigger actions and select entries.

### USAGE

    usplash ACTION [OPTIONS]

#### ACTION := m|i|r|d|w|q|S

| Switch | Longswitch   | Argument    | Description                 |
| -----: | -----------: | :---------- | :-------------------------- |
|     -m |       --menu |             | Menu/Daemon Mode            |
|     -i |       --item | KEY[:INDEX] | Set/create item KEY[:INDEX] |
|     -r |        --run | KEY[:INDEX] | Run KEY[:INDEX]'s action    |
|     -d |     --delete | KEY[:INDEX] | Delete item KEY[:INDEX]     |
|     -w |     --widget | KEY         | Set/change widget for KEY   |
|     -q |       --quit | [SCRIPT]    | Stop daemon; [run SCRIPT]   |
|     -S | --switchroot | MOUNTPOINT  | Switch root to MOUNTPOINT   |
|     -h |       --help |             | This message                |

#### OPTIONS for --menu -m
| Switch | Argument     | Description                               |
| -----: | -----------: | :---------------------------------------- |
|     -H |       STRING | Set custom menu-header to STRING          |
|     -F |       STRING | Set custom menu-footer to STRING          |
|     -L |       STRING | Set custom left-tile to STRING            |
|     -R |       STRING | Set custom right-tile to STRING           |

#### OPTIONS for --item -i
| Switch | Longswitch   | Argument    | Description                 |
| -----: | -----------: | :---------- | :-------------------------- |
|     -t |      --title | TITLE       | Set title to TITLE          |
|     -a |     --action | SCRIPT      | Set action to SCRIPT        |

#### OPTIONS for --switchroot -S
| Switch | Longswitch   | Argument    | Description                 |
| -----: | -----------: | :---------- | :-------------------------- |
|     -a |     --action | SCRIPT      | Run SCRIPT after the switch |

### Licensed under GNU GPLv3

usplash is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

usplash is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this software; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
Boston, MA 02111-1307 USA

http://www.gnu.org/licenses/gpl.html
