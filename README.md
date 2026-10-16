# Embedded-Security-Project

plan attaque 
commencer a expliquer comment on fait la connexion 
expliquer les différentes faille une breve intor e t ce que on a trouverr, rajouter des graphique fais avec jupyternotebook 
expliquer comment contrer les failles 


## Introduction 
This repository contains the work done for the Embedded Security Project, a course given in the 5MEO program at ECAM Brussels Engineering School.

The main objective of this project is to analyze and attack an Arduino Uno R3 from a hardware security perspective. The Arduino contains an unknown program protected by a password, and my goal is to find vulnerabilities that could allow us to bypass this protection.

To achieve this, we will explore different attack techniques, mainly Timing Attacks, Power Analysis, and Voltage Glitching, using tools such as the ChipWhisperer Nano.

After identifying and exploiting potential vulnerabilities, we will also investigate possible countermeasures to improve the security of the system.

This repository documents my approach, experiments, results, and conclusions throughout the project.

## Python environment

Use Python 3.11 for this project. Python 3.14 currently installs an experimental
MinGW-W64 NumPy build on Windows, which can crash while importing ChipWhisperer.

Create the environment once from PowerShell:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
```

Run the ChipWhisperer connection check with:

```powershell
.\.venv\Scripts\python.exe to_delete.py
```

The scripts that use the UART connection can be run with the same
`.venv\Scripts\python.exe` command.

### Timing attack 
The first attack will be to exploit the repsonse time of

Our first approach was to perform a timing attack through the UART interface, without using any prior knowledge of the password.

We tested all 94 printable ASCII characters as possible first characters. Each candidate was followed by a fixed-length padding sequence and tested 30 times.

The measured response times were approximately 43.4 ms, with standard deviations ranging from around 56 to 135 µs among the highest-ranked candidates.

The character with the highest median response time was `G`. However, the difference between the highest-ranked candidates was only a few microseconds and was not statistically relevant to be used. I make some other mesurement and each time the scirpt give me totally different result. 

We therefore concluded that measuring the response time through Python and USB-UART was not sufficiently precise to identify the correct character reliably.

Our next approach will use the ChipWhisperer Nano to obtain measurements synchronized with the execution of the password verification routine.
The advantage is we will be less vunerable of electrica bruit

### ChipWhisperer capture timeout

`scope.capture()` waits for a trigger edge; sending bytes over UART alone does
not trigger a capture. For the SPA notebook, connect the firmware trigger
output to `TIO4` on the CW-Nano and configure the scope for `tio4` before
calling `capture_trace`. The firmware must drive this output around the
password-checking code. A timeout therefore means that no trigger edge was
detected, not that the UART response was late. The notebook raises an
actionable error in this case instead of attempting to plot an empty trace.

## early-exit 

One of the vulnerabilities in the code is the early exit during password verification.

The program compares the password character by character and stops as soon as one character is incorrect.

We can exploit this vulnerability using a timing attack or power analysis. The more correct characters we enter, the more instructions the CPU executes. By measuring the execution time or power consumption, we could progressively recover the password.

To counter this vulnerability, we can use a constant-time comparison. Instead of stopping at the first incorrect character, the program always checks the entire password and only returns the result at the end.

We could also add a random delay before sending the response, making timing attacks more difficult. However, this would not prevent an attacker from analysing the CPU's power consumption.

## Power Analysis attempt (ChipWhisperer-Nano)

### Setup

The ATmega328P was desoldered from the Arduino Uno board and placed on a
breadboard, to get a cleaner power measurement point isolated from the rest
of the board's circuitry (USB-serial chip, voltage regulator, LEDs).

The chip still needs support signals that live on the Arduino board itself:

- RESET (pin 1), XTAL1/XTAL2 (pins 9/10), VCC/AVCC (pins 7/20) and GND
  (pins 8/22) were wired back to the corresponding pins of the Arduino's
  now-empty DIP-28 socket, reusing the board's oscillator, reset circuitry
  and power supply.
- D10 (pin 16, RX of the firmware's `comms` channel) and D11 (pin 17, TX)
  were wired directly to the CW-Nano instead, independently of the Arduino
  board.

CW-Nano wiring (20-pin connector):

| CW-Nano pin | Function | Connected to |
|---|---|---|
| 2 | GND | common ground |
| 8 | VREF | chip VCC (5V) |
| 10 | TIO1 (RX) | ATmega pin 17 (D11, firmware TX) |
| 12 | TIO2 (TX) | ATmega pin 16 (D10, firmware RX) |

### Identifying the real communication channel

Reading the firmware source confirmed that the password prompt is not
handled by the hardware UART (`Serial`, 115200 baud, pins D0/D1) — that one
only prints a welcome banner once at boot. The actual password channel is a
`SoftwareSerial` instance (`comms`) on pins D10/D11, running at **9600
baud**, exchanging **plain text** lines (no SimpleSerial framing).

### Communication issue and fix

Initial capture attempts used `cw.target(scope, cw.targets.SimpleSerial2)`
with `target.write()` / `target.read()`, which returned empty responses.
These methods encode/decode the structured SimpleSerial protocol (start
byte, command char, checksum), which this custom firmware does not
implement at all. Switching to the raw serial accessor
(`target.ser.write()` / `target.ser.read()`) fixed communication at the
protocol level — consistent with the Troubleshooting section of the course
wiki.

### Trigger issue (current blocker)

Even with communication working, `scope.capture()` consistently timed out
(`Timeout in cwnano capture()`), independently of whether the UART exchange
itself succeeded. Root cause: `scope.capture()` waits for a hardware
trigger edge on `TIO4`, not for UART activity. The firmware does drive pin
13 (D13) HIGH before the password comparison and LOW right after, which is
a usable trigger source — but this pin was **not yet physically wired** to
TIO4 (pin 16 of the CW-Nano's 20-pin connector) at the time of this
session, so no trigger edge ever reached the capture device.

### Status: not working yet

This attempt did **not** manage to retrieve the secret during this
session; every capture ended in a timeout before any trace could be
recorded. Documented here as a negative result, consistent with the
previous timing-attack attempt: the vulnerability (early-exit password
comparison, see above) is still believed to be exploitable via power
analysis, but requires a proper trigger connection to be observable with
the CW-Nano.

Next steps:

- Wire ATmega pin 19 (D13) to CW-Nano pin 16 (TIO4).
- Configure the scope explicitly: `scope.io.tio4 = 'high_z'`,
  `scope.trigger.triggers = 'tio4'`, `scope.adc.basic_mode = "rising_edge"`.
- Re-attempt trace capture and visually compare traces for different
  candidate passwords before building a full brute-force script.
