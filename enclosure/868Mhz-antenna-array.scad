
include <BOSL2/std.scad>
include <BOSL2/rounding.scad>
include <BOSL2/modular_hose.scad>
include <BOSL2/turtle3d.scad>
include <BOSL2/skin.scad>
include <BOSL2/joiners.scad>
include <BOSL2/threading.scad>

$fn = $preview ? 20 : 75;

DEPTH = 40;        // Hight of the box
PLATE = 2.5;       // Plate thickness
COAX_DIAM = 2.8;   //[2.8:RG178,2.6:2.6]
ANT_DIAM = 2.2;    // Diameter of the antenna
WALL = 0.4 * 3;    // Thickess of walls
SCREW_DIAM = 3;    // Diameter of all attachment screws
RIGHT_OPEN = true; // Ope right side so a folded di-pole can fit
SLOP = 0.3;        // Slop factor for your printer, only applied on the camers screw mount
//

/* [Hidden] */
// Values calculated from  https://www.changpuak.ch/electronics/Dipole_folded.php
// Optmised with 4nec2
ANTENNA_R = 5;                             // Radious of folded antenna bend
ANTENNA_C = 142;                           // Calculated and optmised by 4nec2
HALF_WAVE_LENGTH = 299792458 / 868300 / 2; // 868.3Mhz;

// Case width based on wavelength 868.3Mhz
WIDTH = HALF_WAVE_LENGTH / 2 + 11; // Idially 8.5cm(quarter wave length??) else 65mm

WIDTH_ANT_HOLDER_BLOCK = 6.5; // Size of antenna holder block

// Antenna Ends
back(DEPTH / 2 + 10) xcopies(15, RIGHT_OPEN ? 2 : 4) antennaEnd();

back(50) foldedAntennaHelper();

// singleAntenna();
left(100) lid();
doubleAntenna();

/**
 * Little tool to help fold the folded dipole at the right size
 */
module foldedAntennaHelper()
{
	diff("remove") cuboid([ ANTENNA_C, ANTENNA_R * 2, 4 ], anchor = BOTTOM)
	{
    // Center position marker
		position(TOP) xcopies(4, 3) cube([ 0.2, ANTENNA_R * 2, 0.4 ], anchor = BOTTOM);

    // Hole for screw
		position(BOTTOM) tag("remove") xcopies(ANTENNA_C / 3 - 10, 4) cyl(d = 3, l = 10);
		position(TOP) tag("remove") xcopies(ANTENNA_C / 3 - 10, 4) up(0.01) cyl(d1 = 3, d2=7, l = 2, anchor=TOP);

    // block + removal 
		for (pos = [ LEFT, RIGHT ])
			position(FRONT + pos + BOTTOM) cube([ 10, 3, 10 ], anchor = pos + BACK + BOTTOM);

		tag("remove") position(FRONT + BOTTOM) up(8)
		{
			xcyl(d = ANT_DIAM, l = ANTENNA_C + 1);
			cube([ ANTENNA_C + 1, ANT_DIAM, 4 ], anchor = BOTTOM);
		}
	}

	xcopies(ANTENNA_C, 2) difference()
	{
		union()
		{
			cylinder(r = ANTENNA_R, h = 8);
			cylinder(r = ANTENNA_R - ANT_DIAM / 2, h = 12);
		}
		up(8) torus(r_maj = ANTENNA_R, r_min = ANT_DIAM / 2);
	}
}

// Model with just one antenna (right one).
module singleAntenna()
{
	size = [ 50, DEPTH, PLATE ];
	diff("remove") cube(size, anchor = BOTTOM)
	{
		diff("remove2")
		{
			position(TOP)
			    rect_tube(h = WIDTH_ANT_HOLDER_BLOCK, size = [ size.x, size.y ], wall = WALL, anchor = BOTTOM);
			tag("remove2") position(TOP + FRONT + LEFT) right(6)
			    cuboid([ 4, WALL, 7 ], anchor = BOTTOM + FRONT + CENTER);
		}

		// Balun
		position(TOP) right(12) balunHolder(anchor = BOTTOM, bbox = false);

		// Screw mounts
		position(TOP + LEFT + BACK) right(14) fwd(8) screwMount();
		position(TOP + RIGHT + FRONT) left(14) back(8) screwMount();

		// Holder for wires
		position(BOTTOM + LEFT) back(4) right(3) wireAssembly(anchor = LEFT + BACK + BOTTOM, coaxDiam = COAX_DIAM);

		// Antenna's
		right(21) antennas(folded = RIGHT_OPEN);

		// STandard camera holder screw mount
		position(LEFT) right(7) back(2)
		{
			position(TOP) cyl(d = 12, l = WIDTH_ANT_HOLDER_BLOCK, anchor = BOTTOM);
			tag("remove") position(BOTTOM) down(5)
			    threaded_rod(d = 0.25 * INCH + SLOP, l = 15, pitch = INCH / 20, anchor = BOTTOM);
		}
	}
}

// Model with two antenna's
module doubleAntenna()
{
	size = [ WIDTH, DEPTH, PLATE ];

	diff("remove") cube(size, anchor = BOTTOM)
	{
		diff("remove2")
		{
			position(TOP)
			    rect_tube(h = WIDTH_ANT_HOLDER_BLOCK, size = [ size.x, size.y ], wall = WALL, anchor = BOTTOM);
			tag("remove2") position(TOP + FRONT) cuboid([ 9, WALL, 7 ], anchor = BOTTOM + FRONT);
		}

		mirror_copy(v = [ 1, 0, 0 ])
		{
			// Balun
			position(TOP) right(HALF_WAVE_LENGTH / 4) left(7) balunHolder(anchor = BOTTOM, bbox = false);

			// Screw mounts
			position(TOP) left(25) ycopies(25) screwMount();

			// Holder for wires
			position(BOTTOM) back(4) wireAssembly(anchor = LEFT + BACK + BOTTOM);
		}

		// Antenna's
		right(HALF_WAVE_LENGTH / 4) antennas(folded = RIGHT_OPEN);
		mirror(v = [ 1, 0, 0 ])
		{
			right(HALF_WAVE_LENGTH / 4) antennas(folded = false);
		}

		// Standard camera holder screw mount
		fwd(1)
		{
			position(TOP) cyl(d = 12, l = WIDTH_ANT_HOLDER_BLOCK, anchor = BOTTOM);
			tag("remove") position(BOTTOM) down(5)
			    threaded_rod(d = 0.25 * INCH + SLOP, l = 15, pitch = INCH / 20, anchor = BOTTOM);
		}
	}
}

// Right side antenna assembly
module antennas(folded = false, pos = 0)
{
	position(TOP + FRONT) right(pos) singlePole(bbox = false, anchor = BOTTOM + BACK, spin = 180, openTop = folded)
	{
		if (folded)
		{
			position(BOTTOM) zrot(180) down(PLATE)
			    foldedHolder(h = PLATE + WIDTH_ANT_HOLDER_BLOCK / 2, l = ANTENNA_R * 2);
		}
	}

	position(TOP + BACK) right(pos) singlePole(bbox = false, anchor = BOTTOM + BACK, openTop = folded)
	{
		if (folded)
		{
			position(BOTTOM) down(PLATE) foldedHolder(h = PLATE + WIDTH_ANT_HOLDER_BLOCK / 2, l = ANTENNA_R * 2);
		}
	}

	tag("remove")
	{
		position(TOP + FRONT) right(pos)
		    singlePole(bbox = false, anchor = BOTTOM + BACK, spin = 180, cut = true, openTop = folded);
		position(TOP + BACK) right(pos) singlePole(bbox = false, anchor = BOTTOM + BACK, cut = true, openTop = folded);
	}
}

// Lid
module lid() {
diff("remove") cube([ WIDTH, DEPTH, 2 ], anchor = BOTTOM)
{
	mirror_copy(v = [ 1, 0, 0 ])
	{
		tag("remove") position(BOTTOM) left(25) ycopies(25) cyl(d = SCREW_DIAM, h = 5, anchor = BOTTOM);
	}

	{
		position(TOP + LEFT) right(5) linear_extrude(0.2) text("1/868", font = "Arial Black", size = 5, anchor = LEFT);
		position(TOP + RIGHT) left(5) linear_extrude(0.2) text("2/868", font = "Arial Black", size = 5, anchor = RIGHT);
	}
	back(10)
	{
		position(TOP + FRONT) linear_extrude(0.2) text("GA/TAS", font = "Arial Black", size = 5, anchor = CENTER);
	}
}
}


module antennaEnd(length = 14)
{
	d = ANT_DIAM + 0.4 * 5 * 2;
	diff("remove") cyl(d = d, l = length, anchor = BOTTOM, rounding2 = (d - 2) / 2)
	{
		tag("remove") position(BOTTOM) cylinder(d = ANT_DIAM + 0.5, h = length - 2, anchor = BOTTOM);
	}
}

module wireAssembly(anchor = BOTTOM, spin = 0, orient = UP, center, bbox = false, cutout = false, coaxDiam = COAX_DIAM)
{
	offset = 0;
	LIFTZ = 2;
	LIFTY = 0;

	module draw()
	{

		R4 = cutout ? 0 : 4;
		R2 = cutout ? 0 : 2;

		difference()
		{
			// TODO: Make difference, then send it to wire
			translate([ offset, LIFTY, 0 ])
			wire(rect([ coaxDiam + 0.4 * 4 * 2, PLATE + coaxDiam + LIFTZ ], anchor = FRONT,
			          rounding = [ 0, R2, R4, 0 ]));

			if (!cutout)
				translate([ offset - 0.05, LIFTY, PLATE + LIFTZ ])
			wire(rect([ coaxDiam, coaxDiam + 0.1 ], anchor = FRONT), 0.05);
		}
	}

	fSize = [ 19.5, 77.4, PLATE + coaxDiam + LIFTZ ];

	attachable(anchor, spin, orient, size = fSize, center)
	{
		union()
		{
			if (bbox)
#cube(fSize, center = true);

				translate(fSize / -2)
			right(fSize.x) back(14.5) back(60) mirror([ 1, 0, 0 ]) draw();
		}
		children();
	}
}

module wire(shape, l = 0)
{
	// Minimum bend diameter of the wire
	r = (20 - 2) / 2;
	L = 8;
	path = turtle3d([
		"move",
		2 + l,
		"arczrot",
		r,
		-45,
		"move",
		L,
		"arczrot",
		r,
		-45,
		"move",
		60 + l,
	]);
	path_sweep(shape, path, $fn = 3);
}

module singlePole(anchor = BOTTOM, spin = 0, orient = UP, center, bbox = false, cut = false, openTop = false)
{
	BLOCK_LENGTH = 14;
	module draw()
	{
#back(-4)
		ycyl(h = HALF_WAVE_LENGTH / 2, d = ANT_DIAM, anchor = FRONT);

		if (cut)
		{
			ycyl(h = BLOCK_LENGTH + 5 + 0.1, d = ANT_DIAM + 0.2, anchor = FRONT);
			if (openTop)
			{
				cube([ ANT_DIAM + 0.2, BLOCK_LENGTH + 10, WIDTH_ANT_HOLDER_BLOCK * 2 ], anchor = FRONT + BOTTOM);
			}
		}
		else
		{
			diff("remove_sp")
			    cuboid([ WIDTH_ANT_HOLDER_BLOCK, BLOCK_LENGTH, WIDTH_ANT_HOLDER_BLOCK ],
			           rounding = WIDTH_ANT_HOLDER_BLOCK / 2, edges = [ TOP + RIGHT, TOP + LEFT ], anchor = FRONT)
			{

				tag("remove_sp")
				{
					ycyl(h = BLOCK_LENGTH + 5 + 0.1, d = ANT_DIAM + 0.2);
					position(BOTTOM) up(2) cyl(d = 4, l = 10, anchor = BOTTOM);

					if (openTop)
					{
						position(CENTER) cube([ ANT_DIAM + 0.2, BLOCK_LENGTH + 0.02, WIDTH_ANT_HOLDER_BLOCK * 2 ],
						                      anchor = CENTER + BOTTOM);
					}
				}
			}
		}
	}

	fSize = [ WIDTH_ANT_HOLDER_BLOCK, BLOCK_LENGTH, WIDTH_ANT_HOLDER_BLOCK ];
	attachable(anchor, spin, orient, size = fSize, center)
	{
		union()
		{
			if (bbox)
#cube(fSize, center = false);

				fwd(BLOCK_LENGTH / 2) draw();
		}
		children();
	}
}

// Holder for a folded antenna to add some integrity
module foldedHolder(l = 20, h = 10)
{
	cube([ l, 5, 3 ], anchor = LEFT + BOTTOM);
	diff("foldedHolder_remove") right(l) prismoid(size1 = [ 5, 5 ], size2 = [ 5, 10 ], h = h + ANT_DIAM / 2)
	{
		down(ANT_DIAM / 2) tag("foldedHolder_remove") position(TOP)
		{
			ycyl(d = ANT_DIAM, l = 20);
			cube([ ANT_DIAM, 20, 5 ], anchor = BOTTOM);
		}
	}
}

// Holder for the balun ETC1-1-13 ETC1-1-13TR  1:1
module balunHolder(anchor = BOTTOM, spin = 0, orient = UP, center, bbox = false)
{
	fSize = [ 4 + 1.6, 4 + 1.6, 1.5 ];
	module draw()
	{
		color("orange") difference()
		{
			cube(fSize, center = true);
			up(0.01) cube(fSize - [ 1.6, 1.6, 0 ], center = true);
		}
	}

	attachable(anchor, spin, orient, size = fSize, center)
	{
		union()
		{
			if (bbox)
        #cube(fSize, center = true);

				draw();
		}
		children();
	}
}

module screwMount()
{
	d2 = 5;
	l = WIDTH_ANT_HOLDER_BLOCK;
	SCREW_B_DIAM = 8;
	difference()
	{
		cyl(h = l, d1 = SCREW_B_DIAM, d2 = d2, anchor = BOTTOM);
		cyl(h = 50, d = SCREW_DIAM - 0.75, center = true, anchor = BOTTOM);
	}
}

// Folded dipole
// https://www.nonstopsystems.com/radio/frank_radio_antenna_folded_dipole.htm
// https://k7mem.com/Ant_Folded_Dipole.html
// https://www.kolins.cz/dipole-antenna-with-balun/
// https://www.changpuak.ch/electronics/Dipole_folded.php


// 4NEC2
//CM RVT - Dipole 868.3Mhz
//CE
//SY ANT_C=0.142249
//SY H=0.0
//SY ANT_R=.005
//SY wireRad=0.0011
//GW	1	17	0	0	H	0	0	ANT_C+H	wireRad
//GW	2	17	ANT_R*2	0	ANT_C+H	ANT_R*2	0	H	wireRad
//GW	3	1	0.0000	0	H+0.0000	0.0007	0	H+-0.0025	wireRad
//GW	4	1	0.0007	0	H+-0.0025	0.0025	0	H+-0.0043	wireRad
//GW	5	1	0.0025	0	H+-0.0043	0.0050	0	H+-0.0050	wireRad
//GW	6	1	0.0050	0	H+-0.0050	0.0075	0	H+-0.0043	wireRad
//GW	7	1	0.0075	0	H+-0.0043	0.0093	0	H+-0.0025	wireRad
//GW	8	1	0.0093	0	H+-0.0025	0.0100	0	H+-0.0000	wireRad
//GW	9	1	0.0100	0	ANT_C+H+0.0000	0.0093	0	ANT_C+H+0.0025	wireRad
//GW	10	1	0.0093	0	ANT_C+H+0.0025	0.0075	0	ANT_C+H+0.0043	wireRad
//GW	11	1	0.0075	0	ANT_C+H+0.0043	0.0050	0	ANT_C+H+0.0050	wireRad
//GW	12	1	0.0050	0	ANT_C+H+0.0050	0.0025	0	ANT_C+H+0.0043	wireRad
//GW	13	1	0.0025	0	ANT_C+H+0.0043	0.0007	0	ANT_C+H+0.0025	wireRad
//GW	14	1	0.0007	0	ANT_C+H+0.0025	0.0000	0	ANT_C+H+0.0000	wireRad
//GE	0
//LD	5	1	0	0	58000000
//LD	5	2	0	0	58000000
//LD	5	3	0	0	58000000
//LD	5	4	0	0	58000000
//LD	5	5	0	0	58000000
//LD	5	6	0	0	58000000
//LD	5	7	0	0	58000000
//LD	5	8	0	0	58000000
//LD	5	9	0	0	58000000
//LD	5	10	0	0	58000000
//LD	5	11	0	0	58000000
//LD	5	12	0	0	58000000
//LD	5	13	0	0	58000000
//LD	5	14	0	0	58000000
//GN	-1
//EK
//EX	0	1	9	0	1	0	0
//FR	0	0	0	0	868	0
//EN





//
//var pi = 3.141592653;
//var frequenz = 868.3 ;
//var verkuerzungsfaktor = 0.95; 	    
//var lambda = 299792 / (frequenz * lengthunit);
//
//var lengthunit = 1
//var precision=1000;
//
//      	   
//a = Math.round(lambda * precision * 0.19 * verkuerzungsfaktor) / precision ;
//b = Math.round(lambda * precision * 0.10 * verkuerzungsfaktor) / precision ;
//c = Math.round(lambda * precision * 0.45 * verkuerzungsfaktor) / precision ;
//d = Math.round(lambda * precision * 0.20 * verkuerzungsfaktor) / precision ;
//gap = Math.round(lambda * precision * 0.01 * verkuerzungsfaktor) / precision ;
//r = Math.round(lambda * precision * ( 0.05 / 3.141592653 ) * verkuerzungsfaktor) / precision ;
//rod = Math.round(lambda * precision / 300 ) / precision ;
//total = Math.round(lambda * precision * verkuerzungsfaktor) / precision ;
//		
//console.log("c= "+c)			
//console.log("r= "+r)			
//		// https://www.nonstopsystems.com/radio/frank_radio_antenna_folded_dipole.htm
//console.log("total Length = " + (c*2 + pi*r*2))
//c= 147.6
//r= 5.22
//total Length = 327.99822729732
// this is essentially wavelength * velocity factor = 345.4*0,95

// Function to generate antenna curves
//function generateCircleCoordinates(tag, R, segments, angleStart, angleEnd) {
//  let coords = "";
//  const wireRad = 1; // assuming the wire radius is always 1
//  let step = (angleEnd-angleStart) / segments;
//  // loop through angles and calculate X and Y coordinates for each point on the circle
//  var i = angleStart;
//
//    let angle = angleStart * Math.PI / 180; // convert angle to radians
//    var xPrev = (R/2 + R/2 * Math.cos(angle)).toFixed(4); // calculate X coordinate
//    var yPrev = (R/2 * Math.sin(angle)).toFixed(4); // calculate Y coordinate
//
//  i = i + step;
//  while (i<=angleEnd) {
//    let angle = i * Math.PI / 180; // convert angle to radians
//    let x = (R/2 + R/2 * Math.cos(angle)).toFixed(4);; // calculate X coordinate
//    let y = (R/2 * Math.sin(angle)).toFixed(4);; // calculate Y coordinate
//
//    // append new coordinates to the string
//     console.log(`GW\t${tag}\t1\t${xPrev}\t0\t${yPrev}\t${x}\t0\t${y}\twireRad`);
//    // update previous coordinates for the next iteration
//    xPrev = x;
//    yPrev = y;
//    i = i + step;
//    tag++;
//  }
//
//  // return the string with all coordinates
//  return coords;
//}
//generateCircleCoordinates(9, 0.01,6,0,180)


// Dipole
// https://www.omnicalculator.com/physics/dipole
// Length 160mm
// https://www.mobilefish.com/download/lora/lora_part41.pdf
// 4NEC2

//CM RVT - Dipole 868.3Mhz
//CE
//SY H=10
//SY L=0.160
//SY wireRad=0.0011
//GW	1	17	0	0	H	0	0	H+L	wireRad
//GE	0 
//LD	5	1	0	0	58000000
//GN	2	0	0	0	3	0.0001
//EK
//EX	0	1	9	0	1	0	0
//FR	0	0	0	0	868	0
//EN

