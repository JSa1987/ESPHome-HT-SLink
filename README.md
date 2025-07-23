# ESPHome Home Theather Controller with Sony S-Link support

This is an ESPHome controller I made to fully control my home theather setup from Home Assistant. This contorller includes the following input/outputs that suit my needs, but can be easyly be modified for different setups.
- Sony S-Link Input/Output - Used to controll a Sony CDP-CX225 CD Player
- IR Input - Used to detect ON/OFF and Volume commands from a Roku remote
- IR Output - Used to controll a Yamaha RX-V2700 receiver and a daisy chained IR blaster
- ON/OFF Output (5v) - Used to controll a power conditioner (completly turn off the power to the whole setup)
- 12v ON/OFF Input - Used to detect the ON/OFF status of the Yamaha RX-V2700 receiver
- 5v ON/OFF Input- Used to detect the ON/OFF status of the TV (connected to a USB port of the TV)

## Sony S-Link
An Arduino Pro Mini is used as intrface for the Sony S-Link Bus. The arduino is interfaced with the ESP8622 via a serial connection.
The Arduino sketch (in the 'Arduino Sketch' folder) is entirely based on the great work from [robho](https://github.com/robho/sony_slink). The only change is that the modified code allows the option of communicating over serial using raw Hex bytes. This is achieved by defining the 'HexOutput' option at the beginning of the code (if this is not defined the code behaves as the original code). 

I'm sucessfully using this setup to control and read information from a Sony CDP-CX225 CD Player. The ESPHome yaml file with the configuration for this device is available in the 'ESPHome' folder. 

## Hardware
To physically connect the Arduino to a Sony device via the S-Link bus and the ESP8266 to the other devices (e.g. IR blaster, Power Conditioner etc.) some additional components are needed. See schematic below.
![circuit](Schematics/Circuit.png)

The S-Link bus opeartes at a 5v logic level, while the ESP8266 operates at a 3.3v logic level. There are Arduino boards that operate either at 3.3v or at 5v, therefore the circuit will depend on the type of Arduino board used. The schematics above uses an the 3.3v version of the Arduino Pro Mini, therefore the Arduino can be connected directly to the ESP8266 but R2 and R3 are needed as voltage divider on the S-Link line. If an Arduino that operates at 5v logic level is used instead (e.g. Arduino Nano) then R2 and R3 won't be needed but a logic level shifter is instead needed between the Arduino and the ESP8266.

For the Sony S-Link interface a 3.5 mm mono plug shall be used to connect the circuit to the S-Link/Control A1 port of the Sony device.
A 3.5 mm mono plug would be used also to connect to most IR blasers and IR input ports on receivers, the signal goes on the mono plug tip and the ground on the sleeve.

## Home Assistant interface
### Entities
The ESPHome device exposes the folloewing services that can called from Home Assistant for:
- `esphome.living_room_av_remote_nec` Used to send an IR commands using the NEC protocol. Two parameters are to be provided when calling the service `address` and `command`.
- `esphome.living_room_av_remote_pioneer` Used to send an IR commands using the Pioneer protocol. One parameter is to be provided when calling the service `rc_code_1`.
- `esphome.living_room_av_cdp09` Used to send commands on the the S-Link bys. One parameter is to be provided when calling the service `command`. See the link to www.undeadscientist.com below for the known S-Link commands. 



### Setup for Sony CDP-CX225
For each device on the S-Link bus specific sensor will need to be defined in the ESPHome yaml configuration file. This is done using the ESPHome [UARTX custom component](https://github.com/eigger/espcomponents). See the link to www.undeadscientist.com for the expected responses from S-Link devices. The ESPHome `configuration.yaml` file in the 'ESPHome' folder includes the configuration for a CDP-CX225 CD Player, but can be easily adjusted for other Sony devices.
The following template sensors are to be defined in this file:
- cdp_album => This will reflect the title of the CD loaded.
- cdp_artist => This will reflect the artist of the track playing
- cdp_track_title => This will reflect the title of the track playing
- cdp_bar => This will reflect the current position of in the track playing as precentage of the track duration. 
```
template:
  - sensor:
      - name: "CD Album"
        unique_id: cdp_album
        state: |
          {% from "cdp_cd_list.jinja" import cdp_cd_list %}
          {{ cdp_cd_list.cd[(states("sensor.living_room_av_cd_playing")|int)-1].Album}}
  - sensor:
      - name: "CD Artist"
        unique_id: cdp_artist
        state: |
          {% from "cdp_cd_list.jinja" import cdp_cd_list %}
          {{ cdp_cd_list.cd[(states("sensor.living_room_av_cd_playing")|int)-1].Artist}}
  - sensor:
      - name: "CD Track Title"
        unique_id: cdp_track_title
        state: |
          {% from "cdp_cd_list.jinja" import cdp_cd_list %}
          {{ cdp_cd_list.cd[(states("sensor.living_room_av_cd_playing")|int)-1].Tracks[(states("sensor.living_room_av_playing_track")|int)-1]}}
  - number:
      - name: "CDP Track bar"
        unique_id: cdp_bar
        state: '{{ ((states("sensor.living_room_av_track_positon_s")|int) / (states("sensor.living_room_av_track_length_s")|int) * 100)|int}}'
        step: 1
        set_value: 
          - service: script.av_cdp_jumptime
            data:
              jump_to: "{{ value }}"
```

### Card for Sony CDP-CX225

## Additional resources
The S-Link_CDP-CX225.xlxs file in the 'SLink Supported Commands' folder lists the commands I have found work with the CDP-CX225.
The ESPHome yaml configuration file I use for my setup is provided as reference in the 'ESPHme' folder. The [UARTX custom component form eigger](https://github.com/eigger/espcomponents) is used to interface with the Arduino managing the S-Link bus.

----

## Reference documents:
* http://web.archive.org/web/20070720171202/http://www.reza.net/slink/text.txt
* http://web.archive.org/web/20070705130320/http://www.undeadscientist.com/slink/
* http://web.archive.org/web/20180831072659/http://boehmel.de/slink.htm
