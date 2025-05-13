# Understanding Geodetic Heights: Notes and Clarifications

Heights and reference systems in geodesy and aviation can be confusing. Here is a summary of what I’ve learned so far.

---

## Reference Models

- **WGS84**: The World Geodetic System 1984 is a global ellipsoidal model of the Earth. It approximates sea level but does not account for local gravity variations. Commonly used by GPS.  
  [Wikipedia – WGS84](https://en.wikipedia.org/wiki/World_Geodetic_System)

- **EGM96 / EGM2008**: Earth Gravitational Models that define the geoid—an equipotential surface representing mean sea level—used to approximate "true" elevations.  
  [Wikipedia – EGM](https://en.wikipedia.org/wiki/Earth_Gravitational_Model)

- **Geoid Height (Geoid Undulation)**: The offset between the geoid (e.g., EGM96) and the ellipsoid (e.g., WGS84). This is a locally varying value.

- **Orthometric Height**: Height above the geoid (i.e., mean sea level). This is what we typically refer to as “elevation”.  
  [Wikipedia – Orthometric Height](https://en.wikipedia.org/wiki/Orthometric_height)

- **Geoid Separation**: Another term for geoid height. It is the distance between the WGS84 ellipsoid and the local geoid surface. This value is reported in field 11 of the NMEA GGA sentence.

---

## GPS GGA Messages (NMEA)

- **Field 9 – Altitude**: Orthometric height (i.e., height above mean sea level), typically based on EGM96 or EGM2008.
- **Field 11 – Geoid Separation**: The offset between the WGS84 ellipsoid and the geoid at that location (i.e., geoid undulation).

---

## Protocol-Specific Altitude References

| Protocol       | Altitude Reference        |
|----------------|---------------------------|
| FLARM          | WGS84 Ellipsoid           |
| OGN            | Ortho height              |
| ADS-L          | WGS84 Ellipsoid           |
| FANET          | WGS84 Ellipsoid (not 100% confirmed) |
| ADS-B (online) | WGS84 or Barometric, with QNH if available |
| GDL90 (e.g., ForeFlight) | Can specify either WGS84 or MSL (via capability flags). OpenAce uses bit 0 = 0 to indicate WGS84 Ellipsoid. |

---

## Calculating Altitudes

For OpenAce, GGA messages should report altitude in meters relative to the WGS84 ellipsoid. To convert ellipsoidal height to mean sea level (MSL):

MSL (Orthometric Height) = Ellipsoid Height − Geoid Separation
Ellipsoid Height = MSL (Orthometric Height) + Geoid Separation

This allows you to present GPS-derived altitude in a way that aligns with traditional barometric references.

---

## What Do EFBs Expect?

Most Electronic Flight Bags (EFBs) prefer to receive **altitudes relative to mean sea level (MSL)**. This typically means using EGM96-derived orthometric heights, which closely match barometric altitude in calm weather conditions.

---

## References

- [Mean Sea Level, GPS, and the Geoid – Esri](https://www.esri.com/about/newsroom/arcuser/mean-sea-level-gps-geoid)
- [UNAVCO Geoid Height Calculator](https://www.unavco.org/software/geodetic-utilities/geoid-height-calculator/geoid-height-calculator.html)
