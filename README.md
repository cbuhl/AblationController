# AblationController

## Pins used
### Display
The cable has pairwise twisted connections, so one have to be careful:


|  |  |  |  |  |  |  |  |  |  |  | | | RED CABLE |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| +5v | GND | pin 46 | V_{contrast} | pin 47 | GND | NC | NC | NC | NC | pin 51 | pin 50 | pin53 | pin 52 |

### Stepper motors
X axis: Dir: pin 5, Step: pin 6., Endstop: pin 7.
Y axis: Dir: pin 3, Step: pin 2. Endstop: pin 4.

### TTL output
Presently, it is connected to pin 13.

### Joystick
X axis: A0.
Y axis: A1.

## The stages:

The stage called 17DRV114 moves 44.00mm in 112714 steps. This gives 2561 steps/mm. <br>
The stage called 17DRV113 moves 26.12mm in 84273 steps. This gives 3226 steps/mm.

Along X, For a resolution of 256 px/2.5mm, this gives 5/512 mm/px, with a stepsize (steps/px) of 3226steps/mm x (5/512)mm/px = 31.5 steps/px.

Along Y, for the same resolution, it's 2561 x 5/512 = 25 steps/px. 
