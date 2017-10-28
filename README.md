## usplash - a fancy dynamic menu

usplash is a tiny dynamic menu application for the Linux command-line.

It was written specifically for gear0x's splash-screen menu
and is meant as a script-supported init process.

When you run usplash in menu mode (-m) the main-screen will pop up instantly,
now you can add menu items and scripts they should execute.

Opposed to dialog and other menu systems for the command-line, usplash can be changed at runtime;
meaning that background-processes can add and update entries in the menu, or even trigger actions and select entries.

  usage$ usplash ACTION [OPTIONS]

  ACTIONS: (*optional)
    -m --menu                Menu/Daemon Mode
       * -S "PATH"         Switch root to PATH
       * -a "SCRIPT"       Run after switch
       * -e "SCRIPT"       PRE-EXEC script
       * -H "VALUE"        Menu Header
       * -F "VALUE"        Menu Footer
       * -L "VALUE"        Left Tile
       * -R "VALUE"        Right Tile
    -q --quit "SCRIPT"       Stop daemon and run (optional) SCRIPT
    -i --item "ID"           Set/create item ID (requires -a -t)
    -r --run "ID"            Run action from item ID (
    -d --delete "ID"         Delete item ID
    -h --help                This message.

  OPTIONS:
    -t --title "TITLE"       Set TITLE of item ID (-i)
    -a --action "SCRIPT"     Set SCRIPT of item ID (-i)";

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
