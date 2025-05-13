# Adding Support for a New GPS Type

When integrating a new type of GPS receiver, there are two key requirements to ensure proper functionality:

- **Update Rate**:  
  The GPS must provide a consistent update rate of 2 Hz (2 updates per second). This is currently a fixed expectation within the system due to filtering mechanisms that depend on this rate.

- **Data Format**:  
  The NMEA GGA sentence must report altitude relative to the **WGS84 datum**, and include the **geoid separation** field. Accurate vertical positioning relies on this information.

### Additional Notes

While any GNSS constellation is technically supported, it is **recommended** to use receivers that support **GPS and Galileo**, with **SBAS/WAAS** enabled for enhanced accuracy and reliability.
