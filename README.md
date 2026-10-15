# FlappyBird Clone
This is the nintendo switch version of the FlappyBird remake i've made!

All code & assets (sprite and sound) are made by me.

Font by chequered

## Installation.

To install the software on a switch you need to use nxlink (press Y in the Homebrew channel) :
```bash
nxlink -a IP_SWITCH DIRECTORY_OF_THE_GAME/flappybird.nro -s
```
It will appear in Homebrew channel as "nintendoswitch" with no icon and no author. 

## Build instructions.
This project use Switch SDL2 (with SDL2_image, SDL2_ttf, SDL2_mixer) and the devKitPro SDK.

To build the project you just need to type `make`