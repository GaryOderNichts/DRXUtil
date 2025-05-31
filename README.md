# DRXUtil
DRC/DRH experiments.

> [!CAUTION]
> Modifying the DRC firmware can cause permanent damage.  
> No one but yourself is responsible for any sort of damage resulting from using this tool.

## Features
### Show DRC/DRH information
This screen will display general information about the DRC and DRX.

### Flash firmware
This option allows flashing a new firmware to the Gamepad (DRC).  
DRXUtil comes with built-in patches which can be applied to the original firmware. These patches allow writing to EEPROM values which are usually inaccessible. The gamepad startup screen is modified to show "Modified Firmware" while this firmware is installed. After modifying EEPROM values, this firmware is no longer necessary and the original firmware can be flashed back.  
The original firmware can be flashed back directly from the MLC.  
Additionally a custom DRC firmware image can be flashed from the SD card.

### Set region
This option will use the built-in firmware patches to write to the region value in the EEPROM, effectively region changing the gamepad.  
After modifying the region, the modified firmware is no longer required.

### Edit device info
This option allows editing the device information field in the EEPROM, using the built-in firmware patches.  
The device information field allows changing the Gamepad's production type to development. When in development mode, a DK menu can be opened by holding down the L and ZL buttons on the Gamepad during startup. This menu allows setting the region directly on the gamepad itself.  
Additionally. this option allows changing the shipment status. When the shipment status is set to unshipped, the Gamepad will only boot into the Service Menu where various diagnostics options can be seen. To get out of the Service Menu navigate to "UIC Menu" and select "Write Shipping Flag On" using the A button. Use the + (START) button to execute it. This will restore the shipping flag to shipped.
After modifying the device info, the modified firmware is no longer required.

## Building
For building you need: 
- [wut](https://github.com/devkitPro/wut)
- [libmocha](https://github.com/wiiu-env/libmocha)
- wiiu-sdl2
- wiiu-sdl2_ttf
- wiiu-sdl2_gfx

To build the project run `make`.

## See also
- [drc-fw-patches](https://github.com/GaryOderNichts/drc-fw-patches)
