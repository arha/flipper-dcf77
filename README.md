# Flipper DCF77
Now with 1.4.3 FZ superpowers!

Sends the DCF77 time signal on Flipper's 125khz LFRFID antenna. This lets you update [radio clocks](https://en.wikipedia.org/wiki/Radio_clock). Defaults to 77.5khz. The baseband signal is output on C3.

Building a portable antenna for the LF/VLF band is left as an exercise for the reader.

# Features

* LF frequency configurable 60-200khz
* LF test mode signal
* Supports multiple time signals: DCF77, WWVB and MSF
* Improved debugging: configurable GPIO output: baseband & RF, LED, buzzer and of course - SubGHZ output
* GPIO baseband / RF output, LED preview and buzzer tone selection for debugging

# technical details

### Now with ~~more clicks needed to make it run~~ a real flipper UI!

* It works on every clock I own _eventually_. DCF77 is slow, it sends a complete update once per minute. Sometimes it works on the first try, sometimes I have to wait more than 5 attempts.
* The baseband signal encodes 1 as an 800ms mark, 0 as a 900ms mark. The minute marker on second 59 lasts 1000ms. There's also a PSK modulation which FZ is not doing.
* The transmitter is not off between marks, but is still transmitting at reduced power. This is rarely visible outside Germany. FZ is also not doing this.
* The antenna is highly mistuned for this purpose (sending 77500Hz on a 125000Hz aerial is about 33% off).
* Debug mode: OOK (carrier) and FSK (FM) on 433.670 - tune in with a NFM receiver

# todo

* configurable simulated data (encoding the time `25:69` is possible)
* simulate it just as a timezone offset (for changing clocks around your house according to your country's choice of DST madness)
* Test it on citizen stuff: DCF77 ☑, WWVB ☑, MSF ☑, BPC ☐, JJY ☐. ALS162 is going to be tricky with its phase modulation. RBU might work
