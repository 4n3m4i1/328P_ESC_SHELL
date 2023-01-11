# ESC Shell for ATMega328P
The purpose of this project is to allow simple validation and testing  
of any PWM dependent (ESC, Servo, etc) device via a simple command line  
interface.  
Any 328P board that has `PB6` and `UART` broken out can run the bare  
minimum functionality. Default configuration is `1M BAUD 8N1`.  
  
## Hardware Connections
- `PB6` PWM Output
- `PD7` Trigger Strobe output, toggles on any change to PWM freq or duty
  
## How To
Connecting a board to your computer (FTDI, CH4XX, etc..), then open the  
appropriate port in a decent terminal emulator that supports ANSI ESC Codes.  
Recommendations:
- Linux: `Minicom`
- Windows: `TeraTerm`  
  
The command line supports 10 commands currently:  
- `OUTPUT {1,0}`
- `FREQ {FLOAT} [Hz]`
- `PERIOD {FLOAT} [us],(ms, s)`
- `DUTY {FLOAT} [%]`
- `HI_TIME {FLOAT} [us]`
- `STALL {INT} [ms]`
- `TYPE {ESC, SERV}`
- `ZEPTO`
- `mSub`
- `mAdd`
  
Units within `[]` are implicit, and do not need to be provided.  
If units are not mentioned, the entry is unit-less.  
  
Simply typing any command and hitting `ENTER` will interpret and  
execute the command.  
  
Examples will be listed below.
  
## Command Description
- `OUTPUT {1,0}` Turns the output on (1) or off (0), if the output is off PWM parameters can still be configured.  
- `FREQ {FLOAT} [Hz]` Sets the frequency of the PWM output
- `PERIOD {FLOAT} [us]` Sets the PWM output period
- `DUTY {FLOAT} [%]` Sets the duty cycle of the output. Frequency must be set first else output will be 0.  
- `HI_TIME {FLOAT} [us]` Sets the logic `HIGH` time of the output. Frequency should be set first.
- `STALL {INT} [ms]` Blocking delay of `{INT}` milliseconds
- `TYPE {ESC,SRV}` Loads `ESC` or `SERVO` presets, does not change `OUTPUT` state.
- `ZEPTO` Opens the teeny text editor Zepto where sequentially executed programs can be made.  
- `mSub {INT}` Subtracts `{INT}` microseconds from the current high pulse time
- `mAdd {INT}` Adds `{INT}` microseconds to the current high pulse time
  
#### Presets
- `ESC` 400.0 Hz, 1500us high time (center for most ESCs)  
- `SRV` 50.0 Hz, 1500us high time (center for many servos)
  
### Examples and General Text Entry Tips
The only character that matters for functionality is the leading char.  
One can shorten entries drastically at the cost of readability. Entries are not case sensitive.  
Math functions require the first 2 characters of their mnemonic minimum.
  
Examples:
- `OUTPUT 1` == `OUT 1` == `O 1` == `o 1`
- `HI_TIME 1500us` == `hi 1500u` == `h 1500`
- `FREQ 1100.0` == `f 1100`
- `ZEPTO` == `zepto` == `z`  
- `mAdd 200` == `mA 200` == `ma 200`
  
  
## Zepto
Zepto is a very small text editor that allows the user to  
create small programs composed of the above instructions.  
The help menu within Zepto can be presented with `CTRL+A`.  
Zepto can utilize the same interpreter as standard text entry,  
making its execution functionality akin to that of a `.sh` script.  
  
### Zepto Keybinds
- `CTRL+A` Toggle on screen help menu
- `CTRL+X` Exit Zepto. The currently loaded buffer will persist until power off or the user edits the program again.
- `CTRL+R` Run in Place. Interpret the program written in the on screen buffer line by line.
- ` ~ `    Toggle `INSERT` (default) and `OVERWRITE` cursor mode

### Zepto Specific Commands
Zepto offers internal functions, greatly expanding what one can produce in a short script.
These commands MUST follow specific typing convention or undefined behavior WILL occur.  
There is no decent error checking on these and very long loops can result from malformed input.  
- `j LL CC` Jump to line `LL` (01-20) `CC` (01-99) times, where both must be given as 2 char entries (ie. line 5 would be typed `05`)

#### Zepto Command Examples
- `j 04 10` Will jump to line `04` `10` times, jumps are typically placed after a string of commands, if this is the case the total number of instruction string executions would be `11`, as there was an execution before the jumps began. It is a good idea to subtract `01` from the loop counter if you require a specific number of iterations.
  
  
## Known Issues, Bugs, and More
- Floats are awful but this was hacked together too quickly to care
- AS7 linker did not like libraries today, so library contents are just pasted in.... looks awful  
- Input error rejection really doesn't check beyond the lead char. Malformed entries will be rejected, but no indication will be made that this has happened.  
- Kinda same as above, but a general lack of presenting errors to the user.  
- Realtime compiled program mode not sorted.  
- Binary size is too large (approx 6.5kB) (back to the float issue a bit here...)
- Interpret speed is laughably slow for a lot of reasons, probably won't change though

## Future Additions  
- ultra tiny file system support
- SD Card interface for storing and recalling programs  
- Capability to record timing data?
