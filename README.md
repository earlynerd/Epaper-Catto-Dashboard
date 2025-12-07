# Epaper Cat Tracking Dashboard
![image of epaper dashboard using E1002 and Petkit account](resources/dashboard_1002_petkit.jpg)
![image of epaper dashboard using E1001 and Whisker account](resources/dashboard_1001_Whisker.jpg)
Simple Multi-Cat Dashbord built with Seeedstudio reterminal epaper displays E1001 or E1002. Compatible with PetKit smart litterboxes, and Whisker Litter Robot 4.

It collects data directly from petkit/whisker servers to help track pet weight and liitterbox usage patterns, and display litterbox status info. 

It should support up to four cats, and requires no setup besides using the captive portal to enter your wifi details and smart litterbox account login details. 

It stores 365 days of past usage data to its micro SD card, and has selectable plot date ranges that can be selected with the buttons. It connects to Petkit/whisker servers to request the most recent litterbox usage data every 2 hours, and spends the vast majority of time in deep sleep to conserve battery. 

It is able to determine your local timezone automatically, and synchronize itself and the built in RTC using NTP servers. Be sure to add a CR1225 battery to the holder inside, it does not come with one installed.

Whisker accounts without the paid tier have access to only 7 days of historical data, but the plot will grow to contain more data with time. Petkit accounts can access 30 days, and the records contain the duration of each visit.

All settings and history data are read from and recorded to the micro SD card. So, be sure to install one. Must be 64GB or below, formatted FAT32.

The device will host a captive portal to allow you to select your wifi access point and enter the password, and provide your petkit or whisker account login. Alternatively, after first boot, you can eject the micro SD and edit "secrets.json" to provide these details.  