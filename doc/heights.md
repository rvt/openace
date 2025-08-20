# Understanding Geodetic Heights: Notes and Clarifications

[<img src="img/Discord-Logo-Blurple.png" height="25px">](https://discord.gg/xfRNnhMY)

Heights and reference systems in geodesy and aviation can be confusing. Here is a summary of what
I’ve learned so far.

> GA/TAS USes HAE Internally for it's processing and uses the correct alttiude for each protocol by internal calculations to make predictions easer.

---

## Reference Models

- **WGS84**: The World Geodetic System 1984 is a global ellipsoidal model of the Earth. It
  approximates sea level but does not account for local gravity variations. Commonly used by GPS.  
  [Wikipedia – WGS84](https://en.wikipedia.org/wiki/World_Geodetic_System)

- **EGM96 / EGM2008**: Earth Gravitational Models that define the geoid—an equipotential surface
  representing mean sea level—used to approximate "true" elevations.  
  [Wikipedia – EGM](https://en.wikipedia.org/wiki/Earth_Gravitational_Model)

- **Geoid Height (Geoid Undulation)**: The offset between the geoid (e.g., EGM96) and the
  ellipsoid (e.g., WGS84). This is a locally varying value.

- **Orthometric Height**: Height above the geoid (i.e., mean sea level). This is what we typically
  refer to as “elevation”.  
  [Wikipedia – Orthometric Height](https://en.wikipedia.org/wiki/Orthometric_height)

- **Geoid Separation**: Another term for geoid height. It is the distance between the WGS84
  ellipsoid and the local geoid surface. This value is reported in field 11 of the NMEA GGA
  sentence.

---

## GPS GGA Messages (NMEA)

- **Field 9 – Altitude**: Orthometric height (i.e., height above mean sea level), typically based on
  EGM96 or EGM2008.
- **Field 11 – Geoid Separation**: The offset between the WGS84 ellipsoid and the geoid at that
  location (i.e., geoid undulation).

---

## Altitude Definitions

* PALT Pressure Altitude
* MSL Main Sea Level = `ellipseHeight - geoidSeparation`
* HAE Height Above Ellipse =` MSL + geoidSeparation`
* geoidSep

## Protocol-Specific Altitude References

| Protocol       | Altitude Reference                       | Notes                                                            |
|----------------|------------------------------------------|------------------------------------------------------------------|
| FLARM          | HAE                                      |                                                                  |
| OGN            | MSL                                      |                                                                  |
| ADS-L          | HAE (ADS-L.4.SRD860.G.1.7)               |                                                                  |
| FANET          | MSL                                      |                                                                  |
| ADS-B (online) | HAE or Barometric, when QNH if available |                                                                  |
| GDL90          | HAE or PALT                              | There is a capability switch that can switch between HAE and MSL |
| FLARM PFLAA    | Relative altitudes from ownship          |                                                                  |

> To calculate from AHE to MSL a Orthometric height needs to be applied from EGM96 or EGM2008.

## EFB Requirements

* Sky Demon
    - GDL90 HAE
    - GPS MSL + GeoidSep using $G_RMC, $G_GGA
    - FLARM PFLAA relative altitudes
* ForFLight
    - GDL90 HAE and/OR PALT

---

## Calculating Altitudes

For GA/TAS, GGA messages we use altitude MSL and the geodicHight

`MSL (Orthometric Height) = Ellipsoid Height − Geoid Separation`
`Ellipsoid Height = MSL (Orthometric Height) + Geoid Separation`

This allows you to present GPS-derived altitude in a way that aligns with traditional barometric
references.

Internaly all altitudes are converted to Height Above Elippsoid, this makes it far easer to show
relative altitudes and have correct estimates if a plane in distance is above or below us.

---

## What Do EFBs Expect?

Most Electronic Flight Bags (EFBs) prefer to receive **altitudes relative to mean sea level (MSL)**.
This typically means using EGM96-derived orthometric heights, which closely match barometric
altitude in calm weather conditions.

---

## References

- [Mean Sea Level, GPS, and the Geoid – Esri](https://www.esri.com/about/newsroom/arcuser/mean-sea-level-gps-geoid)
- [UNAVCO Geoid Height Calculator](https://www.unavco.org/software/geodetic-utilities/geoid-height-calculator/geoid-height-calculator.html)
- [CalTopo](https://caltopo.com/map.html#ll=53.33415,6.06445&z=7&b=mbt)
- [what is the geoid](https://support.virtual-surveyor.com/support/solutions/articles/1000261346-what-is-the-geoid-
- [gps versus barometric altitude the definitive answer](https://xcmag.com/news/gps-versus-barometric-altitude-the-definitive-answer/)
