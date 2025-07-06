FROM: https://pastebin.com/raw/nmi71bTG

FLARM projection of aircraft trajectory into the future
-------------------------------------------------------

It was already known that the FLARM collision-avoidance devices transmit data packets
that include the North-South and East-West components of the aircraft velocity at 4
future time points, as projected based on current behavior.

What was not known is what these time points are.

Also the assumptions behind the projection are not published, but thought to be that,
if the aircraft is turning, it will likely keep turning at the same rate.  That is the
most common behavior of a glider circling in a thermal.

A research paper by Baumgartner and Maeder, published in 2009:
"TRAJECTORY PREDICTION FOR LOW-COST COLLISION AVOIDANCE SYSTEMS"
is available online and clearly describes that approach.
It also notes that knowing the prevailing wind, and taking it into account,
is essential.  Using the ground-referenced position data from GNSS, the path of
a circling glider is drifting with the wind, and it thus not a circle.

If corrected for the wind, i.e., computing within the air rather than ground frame of
reference, the aircraft trajectories (under the assumption of steady circling) will be
circular, and the computing of collisions become much simpler.  But, estimating the
wind based on GNSS data while drifting in the wind, does require additional computation
that is more complex than the collision prediction itself.  It is not clear whether the
early FLARM devices, which were very limited in computational power, could do that.

In 2015 FLARM introduced new software and communications protocols, and it was
expected that they have implemented the concepts from that paper, at least in the
later hardware models.

The fact that FLARM transmits the projections as pairs of N-S and E-W velocity components
in discrete time points, rather than, e.g., the aircraft speed and rate of turn, hints
that it already had the wind vector subtracted, thus transmitting the air-reference
velocities.  On the receiving device it can then be directly subtracted from the
(similarly wind-corrected) velocity components of the other aircraft, resulting in the
relative velocity vector which directly leads to the prediction of possible collisions.
Perfect, and cheap in computing resources.

But as it turns out, that is *NOT* what FLARM devices transmit.  Or at least the few
PowerFLARM devices we tested.

Hardware and software were set up to receive and record the data sent from FLARM devices
carried in circling gliders.  Not the official collision-prediction data sentences output
by the devices to compatible display units, but the actual data carried in the radio
bursts.  Here is what we found:

THE TIME POINTS

The 4 times points are the mid-points of 4 future periods, each 3 seconds long.  I.e.,
the projected velocities are at 1.5 sec, 4.5 sec, 7.5 sec, and 10.5 sec from "now".
This is a bit surprising since collision predictions extend at least 18 seconds ahead,
according to FLARM documentation.

As it turns out, given 4 such future velocities along a steady circle, additional ones
can be computed fairly easily like this:  If we have ns[] and ew[] components for the
4 time points indexed 0 through 3, then compute:

  speed = Hypotenuse(ns[0], ew[0])
  speedchange = Hypotenuse(ns[1]-ns[0], ew[1]-ew[0])
  ew[4] = ew[0] + (2 - (speedchange/speed)^2) * (ew[3]-ew[1])
  ns[4] = ns[0] + (2 - (speedchange/speed)^2) * (ns[3]-ns[1])

Thus 4 points is exactly enough to allow cheap computation of additional ones.  So
presumably the receiving FLARM computes 2 more 3-second periods to cover 18 sec total?

THE PROJECTION FOR THOSE TIME POINTS

Examination of the N-S and E-W pairs of velocities shows that the scalar speed at the
4 time points is predicted to stay the same, and the direction of travel is predicted
to rotate by the same amount in each 3-second time period.  Subject to round-off errors
of course, using integer quantities of limited precision.  For example, the following
4 pairs of velocities (here in units of 1/16 of m/s):

  N-S: 388, 284, -100, -384
  E-W: 104, -280, -388, -104

translate to scalar velocities of about:

  402, 399, 401, 398

(essentially constant), and travel azimuths of about:

  15, -45, 256, 195
  
The differences between the latter are all 60 degrees, i.e., the aircraft in this example
is predicted to maintain a turn rate of 20 degrees per second - an 18-second turn, typical
for gliders in thermals.

HOW PROJECTIONS CHANGE AS THE AIRCRAFT PROCEEDS AROUND THE CIRCLE

So far what we've found is simple and expected.  The surprise comes when looking at the
similar projection transmitted by the same FLARM from the same aircraft a few seconds
later.

If there is no wind, then yes, the speed and turn rate remain roughly the same,
as long as the pilot maintains a steady turn.

But in the presence of wind, the projected constant speed - for all the future points in
each separate projection - is higher when the aircraft is currently flying downwind, and
lower when it is currently flying upwind.  Moreover, the predicted rate of turn changes:
all the future points are predicted to follow a lower rate of turn when the aircraft is
currently flying downwind, and a higher future rate of turn if currently flying upwind.

Each transmitted FLARM packet contains 4 pairs of future velocity components.  Those
components are based on the current groundspeed and track, and they assume that there
is no wind.  The next packet does the same thing, but now based on the new current
groundspeed and track.  The new groundspeed and track are not what you would predict
from the previous packet, because the actual wind is not zero.  The resulting numbers
that are sent are therefore incorrect for both the ground and the air frames of reference.
As one famous physicist said, it is "not even wrong"!

HOW CAN THESE PROJECTIONS BE USED FOR COLLISION PREDICTION?

This raises the question: do the FLARM devices correctly predict collision paths of
circling gliders in the presence of wind?  Clearly the data is not transmitted in a
format that would make that easy to do.  Is it possible that the FLARM devices ignore
the wind when doing the computations on the receiving device?  Since gliders often
circle in wind velocities that are a significant percentage of the glider airspeed,
their trajectories (in ground reference) are very different from circles, turning
tightly upwind and opening up downwind.  Look at a soaring bird circling in a
breeze for a demostration of that.  Collision predictions that ignore this would
be of poor quality.

Perhaps very old FLARM devices do that poor calculation, and more recent models
add a correction for wind?  Here is how such a correction can be done, based on
the imperfect data format offered:

Compute the direction of travel implied by each of the first two pairs of N-S and
E-W velocity components.  This requires something like the ATAN2() function.

Compute the difference between these two directions.  This is the turn angle in
each 3 seconds.  Call this ground-reference rate of turn G.  Note that it is
actually only correct for "right now", not for the future time points depicted.

Subtract half that difference from the direction of the first velocity vector.
This results in the implied direction of the velocity vector "right now".

Compute the scalar speed from the two velocity components of one time point,
this requires something like a square root function.

Compute the N-S and E-W velocity components for "right now", from the direction
and the scalar speed.  This requires something like cos() and sin() function calls.

The N-S and E-W components of the wind vector also need to be known, from a separate
estimation procedure.

Compute the scalar wind speed, call it W.

Also compute the direction of the wind, and subtract it from the direction of travel
"right now", to get the "angle from downwind", call it D.  D=0 when traveling
directly downwind, D=180 degrees when traveling directly upwind.  (At, and only at,
those two points the direction of travel is the same in both frames of reference.)

Subtract the wind components from the ground-referenced velocity components to get
the air-reference velocity components, N-S and E-W.

All this had to be done for the "right now" time point because the ground speed at that
point in time (and angle to the wind) is what the given data is implicitly based on.

Now can compute the scalar airspeed, this requires something like a square root function.
Call the airspeed S.  Also compute the air-reference direction of travel "right now",
which differs from the ground-reference direction, unless they happen to be directly
up- or down-wind.

Compute the air-reference rate of turn R:

  R = G * (1 + cos(D)*W/S)

This (constant) rate of turn differs from the (variable) rate of turn G in the ground
frame of reference.  (We used G at the "right now" time point to compute this.)

All the remaining computations are done in the air frame of reference:

Use this rate of turn, and the direction of travel "right now", to get the direction
of travel 3 seconds later (for example).  Assume a constant scalar AIRspeed.

Similarly for any number of additional time points.

Use these air-reference velocities, computed for both the transmitting aircraft and
the receiving aircraft, to project the future changes in their relative position.

All this requires a lot more trigonometry than would have been needed had the data been
transmitted in a format more directly applicable, i.e., in the air frame of reference.
And note that this way only the first 2 of the 4 points are needed.  The fact that 4
are transmitted hints that this complex wind correction may not be done in the devices.
