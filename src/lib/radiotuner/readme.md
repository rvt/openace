

## Class RadioTuner {}

* Module that handles timings of protocols and configures the Radios accordingly to listen and request for sending on the radio frequencies according to the protocols and required maximum power output
* Module keeps track of what protocols traffic is received on and will prioritize the timeslots based on received data.
* Can configure and control 1..N Radio modules. Usually 1 or 2 will be installed


### Limitations

* All radios must be able to handle all protocols. This module won't be able to decide if a specific radio is not able to handle given modulations or frequencies.

#### General operation




## Class CountryRegulation {}

Class that holds the Zone regulations

Per zone the following information is stored:

 * Frequency
 * Timings when data can be received
 * Delay after CAD (when data was received)
 * Maximum power output in the zone
 * Maximum and minimum time between transmissions

 The class can also decide the current `CountryRegulations::Zone`` to be used  (private method) and what Regulation should be used for a specific protocol.

