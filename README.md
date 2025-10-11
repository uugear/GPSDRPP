# GPSDR++ (GPSDRPP)
GPSDR++ is the software for [VU GPSDR](https://www.uugear.com/product/vu-gpsdr/), and it is based the open-source [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus) project, originally created by Alexandre Rouma (ON5RYZ) and enriched by many talented contributors. We deeply appreciate their dedication and hard work, which form the foundation of this software.

## Usage
Start GPSDR++ from the Application menu, or just run "gpsdrpp" in the terminal. 

![Run GPSDR++](https://www.uugear.com/wordpress/wp-content/uploads/2025/09/launch_gpsdrpp.jpg)

GPSDR++ will start as a full-screen application, and its interface looks like this:

![GPSDR++ Layout](https://www.uugear.com/wordpress/wp-content/uploads/2025/09/GPSDRPP_layout.jpg)

The toolbar at the top contains controls for showing/adjusting SDR parameters (on/off, volume, frequency, tuning mode, SNR etc.). The SDR++ menu is hidden by default — you can tap the button in the upper-left corner to toggle the visibility of the menu and/or sidebar.

The sidebar provides several functional pages, such as Radio, Mode S/ADS-B, GPS, and CLK OUT. You can switch between these pages using the two navigation buttons at the bottom of the sidebar.

The main view contains two tabs — Waterfall and Map — and you can switch between them by tapping on the tab header.

By default, when you are facing the screen:
- The left encoder controls the zoom level of the waterfall view.
- The right encoder controls the tuning frequency.

You can change their functionality by pressing the buttons next to each encoder.

On the radio page, you can turn AGC on/off, adjust the gain, and specify the demodulation mode.

![GPSDR++ Radio](https://www.uugear.com/wordpress/wp-content/uploads/2025/09/radio_page.jpg)

To enable ADS-B reception, go to the Mode S/ADS-B page and tick the checkbox at the top. When aircraft are detected, they will also appear in the Map tab within the main view.

![GPSDR++ ADSB](https://www.uugear.com/wordpress/wp-content/uploads/2025/09/ADSB_page.jpg)

To monitor GPS satellite signal strength, navigate to the GPS page. To show your surrounding geography, activate the Map tab.

![GPSDR++ GPS](https://www.uugear.com/wordpress/wp-content/uploads/2025/09/GPS_page.jpg)

To enable the clock signal output, go to the CLK OUT page and tick the checkbox at the top.

![GPSDR++ CLK OUT](https://www.uugear.com/wordpress/wp-content/uploads/2025/09/CLKOUT_page.jpg)



