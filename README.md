# Roxy Firmware

## Roxy Project

The Roxy Arcade Controller Board (Roxy for short) is a fork of the popular [arcin](https://cgit.jvnv.net/arcin/) designed with a few new goals in mind:
* Pin compatibility with existing commercially available controllers
* On-board support of common-anode RGBs (4 on board, with the ability to daisy-chain more)
* Breakouts for other common RGB strips
* Consolidation of relevant forks into a single repo
* New user-facing configuration and firmware update program

## Firmware

This repo is for the firmware that runs on Roxy. See other repos for additional files.

Where possible and relevant, changes shall be PR'd upstream to support the arcin project.

## License

The entire Roxy project, including firmware, board files, and additional supporting software, is released under the 2-clause BSD license.

## Credits

The original [arcin](https://cgit.jvnv.net/arcin/) project is released under the 2-clause BSD license.  
Copyright (c) 2013, Vegard Storheil Eriksen

Additional arcin changes pulled from:  
[MdxMaxX](https://github.com/MdXMaxX/arcin)  
[Ziemas](https://github.com/Ziemas/arcin)

Code driving the TLC59711 is largely based on the [Adafruit TLC59711 library](https://github.com/adafruit/Adafruit_TLC59711), released under the BSD license.  
Copyright (c) 2012, Adafruit Industries  
All rights reserved.

Selected code and methods from [FastLED]
(https://github.com/FastLED/FastLED), released under the MIT license  
Copyright (c) 2013 FastLED