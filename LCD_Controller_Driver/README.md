This project demonstrates the creation of a device driver for controlling an LCD using device tree overlays on a Raspberry Pi 4.

setting pin map
pin number           module
3(GPIO2, SDA1)       I2C SDA
5(GPIO3, SCL1)       I2C SCL

Steps to Create and Load the Device Driver
1. Write the Device Tree Source (DTS) File
  Create a file named lcd-overlay.dts containing the overlay configuration for the LCD.
2. Compile the DTS File to Create a Device Tree Blob Overlay (DTBO)
  Use the following command to compile the DTS file: dtc -@ -I dts -O dtb -o lcd-overlay.dtbo lcd-overlay.dts
3. Copy the DTBO File to the Overlays Directory
  Copy the generated led-controller.dtbo file to the overlays directory: sudo cp lcd-overlay.dtbo /boot/firmware/overlays/
4. Add the Overlay to the Configuration File
  Edit the /boot/firmware/config.txt file and add the following line: dtoverlay=lcd-overlay
5. Reboot the System
6. Build the Kernel Module
  Compile the kernel module (lcd_driver.c) using the Raspberry Pi kernel headers
7. Insert the Kernel Module
  Use the following command to insert the kernel module: sudo insmod lcd_driver.ko

Unloading the Module
  To remove the kernel module, use the following command: sudo rmmod lcd_driver
