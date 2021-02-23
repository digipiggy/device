# device

# Create a New Firmware Version
Congratulations, you've updated the firmware and now you want to load it onto a digipig. Here are the steps.

1. Update the `PRODUCT_VERSION()` to the latest increment. This is found in the digipiggy-photon.ino file. 
2. Navigate to the [Particle Web IDE](https://build.particle.io/build/new). 
  * Name the new app following the following convention v{x}Firmware.
  * Then click the save button (a folder icon) in the left side menu
3. Click on the libraries button (a bookmark icon) in the left side menu. 
  * Search for the `neopixel` library. 
  * Select the `neopixel` libary and click `include in project`
  * Select your project (app) from the list
4. Remove the boilerplate code in the newly created v{x}Firmware.ino file. Replace it with the contents of the digipiggy-photon.ino file.
5. Click on the cloud icon with a down arrow found to the right of the App name. When you hover over it, it should say "Combine and download firmware binary".
  * This will download a firmware.bin file to your computer. 
6. Navigate to the [Particle Web Console](https://console.particle.io/) 
  * Select the DigiPiggy Product
  * Select Firmware
  * Click the upload button in the top right corner
  * Fill out required information (version #, Title, Description) and select the firmware.bin file from your computer. 
7. To add this firmware to a device, select Devices in the Particle Web Console. 
  * Select the device you wish to update from the list. 
  * Select `Edit` from the top right corner of the screen. 
  * Select the new firmware version from the list, select flash now, and then hit save. 
  * The update should take a couple minutes. 

References
* https://docs.particle.io/tutorials/device-cloud/console/#preparing-a-binary