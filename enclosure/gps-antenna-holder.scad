include <BOSL2/std.scad>
include <BOSL2/rounding.scad>
include <BOSL2/joiners.scad>
include <BOSL2/threading.scad>
include <BOSL2/modular_hose.scad>
include <BOSL2/hinges.scad>
include <BOSL2/rounding.scad>

$fn = $preview ? 32 : 256;

// Diameter of the inner case
DIAMETER=37;    // [20:50]
SQUARE_ROUND=5; // [10:25]

WALL=0.4*5;

BHEIGHT=6; // [5:0.5:15]

// Height of the top screw case
THEIGHT=19;  // [5:25]

// Clearance of the top antenna. L
// eep at least 1mm as the ceramic antenna needs that
ANTENNA_CLERARANCE=1; // [1,1.5,2,2.5,3]

// Friction makes some extra room for a rubber ring to ensure the antenna will stay in position during vibrations
FRICTION_RING=true;

// Notches to lock the cap
NOTCHES=[0, 90, 180, 270]; // [0:359]

/** [Visualisation only] **/
// hangle of the hinge
HINGE_ANGLE=45; // [0:110]

// Position of the top to move it up/down
CAP_POSITION=20; // [-8.5:0.5:25]

// Visualize cropping
CROP=false;

MODULE="TOPGNSS"; // [TOPGNSS, square20_20_5, cdebyte_E108_GN04, No Module]

/* [Hidden] */
_CROP=CROP && $preview;
_DIAMETER=DIAMETER+(0.4*4)*2;
CAP_NOTCHE_OFFSET=1; // distance between bottom cap and start of notch
CAP_BOTTOM_SLACK=0.5;  // Room between cap and bottom
HINGE_ATTACH_LENGTH=23; // Controls the length of the hiunge
KNUCKLE_OFFSET=CAP_NOTCHE_OFFSET;
KNUNCKLE_DIAM=6;       // Size of the nuckle
HINGE_SEGS=5;          // Number of hinge items
HINGE_LENGTH=6;        // Length of each hinge


// Simple cropping function
// c=Size of cropping cube
// t=Location of cropping cube
// r=Rotation angle of cropping cube
module crop(c=100,t=[0,0,0],r=0)
  difference() {
      children();
      translate(t)
      rotate(r)
      cube([c,c,c], anchor=LEFT);
  }



//*** START Place here your visualisation modules
module topGNSS() {
  if ($preview) {
    #cyl(d=38,l=0.6,  anchor=BOTTOM) {
      position(TOP) cuboid([27,27,7],anchor=BOTTOM);
      position(BOTTOM) cuboid([25.5,25.5,4],anchor=TOP);
    }
  }

  SCREW_POS=33; // Distance between the holes
  for(r = [0, 90, 180, 270]) {
    zrot(r)
    intersect("keep","fullkeep")
    diff(keep="fullkeep keep")
    cyl(d=DIAMETER,l=BHEIGHT,anchor=TOP){
      tag("keep") left(DIAMETER-4) cube([DIAMETER,DIAMETER,DIAMETER], center=true);
      tag("remove") left(33/2) cyl(d=2,l=15);
    }
  }
}

module cdebyte_E108_GN04 () {
  right(2) if ($preview) {
    #cuboid([21,20,0.6], anchor=BOTTOM) {
      position(TOP) cuboid([18,18,4.5],anchor=BOTTOM);
      position(BOTTOM) right(2.5) cuboid([18,15.5,3],anchor=TOP, spin=90);
      position(BOTTOM) left(8.5) cuboid([4,9,3.5],anchor=TOP);
    }
  }

  diff()
  rrect( rt=0.1, rs=SQUARE_ROUND, d=_DIAMETER, l=BHEIGHT ,anchor=TOP){
    tag("remove") attach(TOP) up(0.01) cuboid([DIAMETER, 21, BHEIGHT], anchor=TOP);
    tag("remove") attach(BOTTOM) right(15) up(1) cube([10, 10, 3], anchor=TOP);
  }
}
//***STOP Place here your visualisation modules


crop(c=(_CROP?300:0),t=[0,0,0],r=90)
  {
  //*** START Place here your visualisation
  if (MODULE == "TOPGNSS") zrot(45) topGNSS();
  if (MODULE == "cdebyte_E108_GN04") zrot(180) cdebyte_E108_GN04();
  if (MODULE == "square20_20_5") %cuboid([20,20,5], anchor=TOP);
  //***STOP Place here your visualisation


  // Bottom
  diff()
  rrect(rt=0.1, rs=SQUARE_ROUND, l=BHEIGHT, d=_DIAMETER, anchor=TOP) {

    tag("remove") {
      attach(TOP) up(1) rrect(rt=0.1, rs=SQUARE_ROUND-WALL, l=BHEIGHT, d=DIAMETER, anchor=TOP);
      // Gap bottom
      position(BOTTOM+RIGHT) right(1) down(0.1) cube([10,3,WALL],anchor=BOTTOM+RIGHT);
    }
    
    
    attach(BOTTOM)
      for (n = NOTCHES) {
        zrot(n + NOTCHES[1]) left(_DIAMETER/2-0.25) zrot(90) wedge([3, 1+CAP_BOTTOM_SLACK, 5], anchor=BOTTOM+FRONT, orient=DOWN);
      }
  }

  // Cap
  color("#FFA0A0")
  up($preview?CAP_POSITION:THEIGHT)
  diff()

  rrect(rt=2, rs=SQUARE_ROUND, l=THEIGHT, d=_DIAMETER+WALL+CAP_BOTTOM_SLACK, anchor=BOTTOM) {
    attach(TOP) down(0.5) {
      tag("remove")  {
        cylinder(d=DIAMETER-5,h=ANTENNA_CLERARANCE, anchor=TOP);
        down(ANTENNA_CLERARANCE) rrect(rt=2, rs=SQUARE_ROUND, l=THEIGHT, d=_DIAMETER+0.5, anchor=TOP);
      }
    }
    
    tag("remove") attach(BOTTOM) down(CAP_NOTCHE_OFFSET)
      for (n = NOTCHES) {
        zrot(n+ NOTCHES[1]) left(_DIAMETER/2) cuboid([12,4,4], anchor=TOP);
      }
  }



  // Bottom plate
  PLATE_DOWN=($preview?0:5);
  down(BHEIGHT+PLATE_DOWN)
  diff()
  cuboid([HINGE_LENGTH*3+4, 20, 0.2*4], anchor=TOP, chamfer=0.2*4, edges=[BOTTOM+LEFT, BOTTOM+RIGHT, BOTTOM+FRONT, BOTTOM+BACK]) {
    left(HINGE_LENGTH*3-HINGE_LENGTH/2) attach(BOTTOM) knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, segs=HINGE_SEGS, offset=KNUNCKLE_DIAM/2+KNUCKLE_OFFSET, arm_height=0, anchor=BOT+RIGHT, inner=true,knuckle_diam=KNUNCKLE_DIAM);     
  }


  // Hinge 
  LENGTH=_DIAMETER/2+HINGE_ATTACH_LENGTH;
  WIDTH=14;
  down(BHEIGHT+PLATE_DOWN+KNUCKLE_OFFSET+($preview?3.5:15))
  xrot(($preview?-HINGE_ANGLE:0))
  left(HINGE_LENGTH*3-HINGE_LENGTH/2) back(3) 
  xrot(90) 
  zrot(-90) 
  diff()
  prismoid(size1=[KNUNCKLE_DIAM/8,LENGTH], size2=[KNUNCKLE_DIAM,LENGTH], shift=[(KNUNCKLE_DIAM - KNUNCKLE_DIAM/8)/2,0], h=WIDTH, anchor=FRONT+TOP) {
       
       
      position(TOP+FRONT) knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, segs=HINGE_SEGS, inner=false, arm_angle=90, anchor=BOT+RIGHT, orient=UP, spin=-90,knuckle_diam=KNUNCKLE_DIAM, offset=KNUNCKLE_DIAM/2) {

          color("green") attach(BACK) xrot(20) down(1) cuboid([HINGE_LENGTH-1,2.5,2], anchor=BOTTOM);
        
          if (FRICTION_RING) {
            tag("remove") right(HINGE_LENGTH) attach(LEFT) {
              color("orange") %torus(od=6,id=2, anchor=BOTTOM);
              cylinder(d=7,h=1.75);
            }
            
          }

        }

        position(TOP+FRONT) knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, segs=HINGE_SEGS, offset=KNUNCKLE_DIAM/2, inner=false, arm_angle=90, anchor=BOT+RIGHT, orient=UP, spin=-90, knuckle_clearance=1,knuckle_diam=KNUNCKLE_DIAM);


      ATTACH_WIDTH=WIDTH+8;
      echo("With of attachment: ", ATTACH_WIDTH);
      // #position(LEFT+BACK) cube([10,WALL,WIDTH], anchor=RIGHT+BACK); on top..
      color("lightblue")
      position(LEFT+BACK) cube([12,3,ATTACH_WIDTH], anchor=LEFT+BACK) {
        right(WALL) 
        position(LEFT+FRONT) 
        prismoid(size1=[WIDTH/2,WALL], size2=[0,WALL], shift=[-WIDTH/4,0], h=WIDTH/2, anchor=BOTTOM+LEFT, orient=FRONT);
        
        tag("remove") {
          up(4) right(2) attach(FRONT) cyl(d=3, l=10) {
            if ($preview) tag("keep") #position(TOP) cyl(d=6, l=2);
          }
          down(4) right(-1) attach(FRONT) cyl(d=3, l=10) {
            if ($preview) tag("keep") #position(TOP) cyl(d=6, l=2);
          }
        }
      }      
   }
}

module gpsSideAttach(w1=21.5, w2=9, d=3, h=18) {
  diff() {
    cuboid([w1+6, d+3, h]) {
      up(1.5) position(FRONT) tag("remove") cuboid([w1, d, 18], anchor=FRONT);

      position(FRONT+TOP) tag("remove") {
        #cuboid([w1, d+3, KNUNCKLE_DIAM], anchor=FRONT+TOP);
        cuboid([4, d+3, h], anchor=FRONT+TOP);
      }
    }
  }
}


// Round Rect module to set top and side rounding
module rrect(rt=4, rs=2, l=10, d=10, anchor=CENTER, spin=0, orient=UP) {
  r=d/2;
  RT = ((rt>r)?r:rt) - 0.1;
  RS = ((rs>r)?r:rs) - 0.1;
  asize=[d,d,l];
  attachable(anchor, spin, orient, asize) {
    down(l/2)     offset_sweep(round_corners(rect(d), radius=RS), height=l, top=os_circle(r=RT));
    children();
  }
}
