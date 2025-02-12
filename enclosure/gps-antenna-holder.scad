
/* Note this file is work in progress */
/* If you have comments, questions or suggestions do let me know. */

include <BOSL2/std.scad>
include <BOSL2/rounding.scad>
include <BOSL2/joiners.scad>
include <BOSL2/threading.scad>
include <BOSL2/modular_hose.scad>
include <BOSL2/hinges.scad>
include <BOSL2/rounding.scad>
include <NopSCADlib/vitamins/screw.scad>
include <NopSCADlib/vitamins/screws.scad>

$fn = $preview ? 32 : 256;

// Diameter of the inner case
WIDTH=31;    // [20:50]
LENGTH=31;    // [20:50]
DIAMETER = WIDTH<LENGTH?WIDTH:LENGTH;
SQUARE_ROUND=3; // [1:25]

WALL=0.4*4;

BHEIGHT=6; // [6:0.5:15]

// Height of the top screw case
THEIGHT=19;  // [5:0.5:25]

// Clearance of the top antenna. L
// eep at least 1mm as the ceramic antenna needs that
ANTENNA_CLERARANCE=1; // [1,1.5,2,2.5,3]

// Friction makes some extra room for a rubber ring to ensure the antenna will stay in position during vibrations
FRICTION_RING=true;


SCREW_DIST_X=0; // [-15:0.5:15]
SCREW_DIST_Y=0; // [-15:0.5:15]
SCREWS=[
  [-WIDTH/2-SCREW_DIST_X,LENGTH/2+SCREW_DIST_Y],
  [-WIDTH/2-SCREW_DIST_X,-LENGTH/2-SCREW_DIST_Y],
  [WIDTH/2+SCREW_DIST_X,-LENGTH/2-SCREW_DIST_Y],
  [WIDTH/2+SCREW_DIST_X,LENGTH/2+SCREW_DIST_Y],
];

/** [Visualisation only] **/
// SHow any non structual design elements, like the HW of the GPS itself
NON_STRUCTURAL=true;

// Position of the top to move it up/down
CAP_POSITION=20; // [0:0.5:50]

// Position of the plate
PLATE_POSITION=0; // [-20:10:100]


CROP=false;

MODULE="TOPGNSS"; // [TOPGNSS, square20_20_5, cdebyte_E108_GN04, No Module]

/* [Hidden] */
_CROP=CROP && $preview;
HINGE_ATTACH_LENGTH=23; // Controls the length of the hiunge
KNUNCKLE_DIAM=BHEIGHT;       // Size of the nuckle
HINGE_SEGS=5;          // Number of hinge items
HINGE_LENGTH=6;        // Length of each hinge


SCREWD=2.4;
SCREWHD=5;
SCREW_L=8;
SCREWWALL=0.4*2;

//up (6) color([1,1,1,0.1]) #cuboid([22,20,8]);

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
    HEIGHT=6;
   {
    if ($preview && NON_STRUCTURAL) {
      up(HEIGHT)
      #cyl(d=38,l=0.6,  anchor=BOTTOM) {
        position(TOP) cuboid([27,27,7],anchor=BOTTOM);
        position(BOTTOM) cuboid([25.5,25.5,4],anchor=TOP);
      }
    }

    SCREW_POS=33; // Distance between the holes
    up(HEIGHT)
    diff(remove="remove3")
    for(r = [0, 90, 180, 270]) {
      zrot(r) {
        intersect("bounds")
        cyl(d=WIDTH,l=HEIGHT,anchor=TOP){
          tag("bounds") left(25.5/2+30/2+1) cube([30,30,10], center=true);
        }
        tag("remove3") left(33/2) cyl(d=2,l=15);
      }
    }
  }
}

module cdebyte_E108_GN04 () {
    HEIGHT=5;

   up(HEIGHT) right(1) if ($preview && NON_STRUCTURAL) {
    #cuboid([22,20,0.6], anchor=BOTTOM) {
      position(TOP) cuboid([18,18,4.5],anchor=BOTTOM);
      position(BOTTOM) right(2.5) cuboid([18,15.5,3],anchor=TOP, spin=90);
      position(BOTTOM) left(8.5) cuboid([4,9,3.5],anchor=TOP);
    }
  }

//  diff()
//  rrect( rt=0.1, rs=SQUARE_ROUND, d=DIAMETER, l=BHEIGHT ,anchor=TOP){
//    tag("remove") attach(TOP) up(0.01) cuboid([DIAMETER, 21, BHEIGHT], anchor=TOP);
//    tag("remove") attach(BOTTOM) right(15) up(1) cube([10, 10, 3], anchor=TOP);
//  }
}
//***STOP Place here your visualisation modules



module cap_casing(anchor=BOTTOM, spin=0, orient=UP, center ) {

  attachable(anchor, spin, orient, size=[WIDTH,LENGTH,THEIGHT], center) {  
    union() {
    down(THEIGHT/2) 
    diff("remove_cap") 
    rrect(rt=2, rs=SQUARE_ROUND, l=THEIGHT, rect=[WIDTH,LENGTH], anchor=BOTTOM) {
            
      attach(TOP) down(0.8) {
        {
          tag("remove_cap") cylinder(d=DIAMETER-10,h=ANTENNA_CLERARANCE+1, anchor=TOP);
          tag("remove_cap") down(ANTENNA_CLERARANCE) rrect(rt=2, rs=SQUARE_ROUND, l=THEIGHT, 
          rect=[WIDTH,LENGTH]-[WALL*2,WALL*2], anchor=TOP);
        }
      }
    } 
     }
    children();
   }
}


module cap() {
  // Cap
  crop(c=(_CROP?300:0),t=[0,0,0],r=90)
  color("#FFA0A0")
  {
    diff()    
    cap_casing(anchor=BOTTOM) {
      position(RIGHT+BOTTOM) cuboid([0.2, 6, 2], anchor=BOTTOM+LEFT, chamfer=0.2, edges=[RIGHT+TOP, RIGHT+BACK, RIGHT+FRONT]);
    
      position(BOTTOM)
      for (n = SCREWS) {
          left(n.x) back(n.y) cyl(d=SCREWD+0.4*5, l=THEIGHT-2, anchor=BOTTOM) { // d Should be the same as bottom case
          position(TOP) cyl(d1=SCREWD+0.4*5, d2=SCREWD+0.4*5-2, l=1.5, anchor=BOTTOM);
        }
        left(n.x) back(n.y) down(0.01) tag("remove") up(0) screwStuff(struct=2, anchor=TOP, shaftd=2.4, shaftl=THEIGHT-2, orient=DOWN);
      }
    }

  }
}

module base_casing(anchor=BOTTOM, spin=0, orient=UP, center ) {
  attachable(anchor, spin, orient, size=[WIDTH,LENGTH,BHEIGHT], center) {  
      union() {
        diff("remove2", "keep2")
          rrect(rt=0.1, rs=SQUARE_ROUND, l=BHEIGHT, rect=[WIDTH,LENGTH]) {
            tag("remove2") attach(TOP) up(1) rrect(rt=0.1, rs=SQUARE_ROUND-WALL, l=BHEIGHT, 
            rect=[WIDTH,LENGTH]-[WALL*2,WALL*2], anchor=TOP);
        }
      }
      children();
   }
}

up($preview?CAP_POSITION:50) cap();
if ($preview) {
  right(3+PLATE_POSITION) down(BHEIGHT/2-2) zrot(-90) bottom_plate();
} else {
  left(50) bottom_plate();
}
casing();


module casing() {
  crop(c=(_CROP?300:0),t=[0,0,0],r=90)
  {

    // Bottom
    diff() {
      base_casing() {

        position(BOTTOM) {
          //*** START Place here your visualisation
          if (MODULE == "TOPGNSS") zrot(45) topGNSS();
          if (MODULE == "cdebyte_E108_GN04") down(0) zrot(180) cdebyte_E108_GN04();
          if (MODULE == "square20_20_5") %cuboid([20,20,5], anchor=TOP);
          //***STOP Place here your visualisation
        }
        
        // Gap bottom for wire
        tag("remove") position(BOTTOM+RIGHT) right(1) down(0.1) cube([11,3,WALL],anchor=BOTTOM+RIGHT);

        // Hinge
        position(RIGHT)
          knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, arm_angle=90, orient=RIGHT, segs=HINGE_SEGS, offset=KNUNCKLE_DIAM/2+0.5, 
            arm_height=0,  anchor=BOT,  spin=90, inner=true, knuckle_diam=KNUNCKLE_DIAM, pin_diam=2) {
            position(BOT) cuboid([1.5,KNUNCKLE_DIAM,HINGE_LENGTH*(HINGE_SEGS-2)], anchor=LEFT, orient=RIGHT);
            
            // Ratched 
            if (FRICTION_RING) {
              for (p = [0, HINGE_LENGTH*2]) {
                right(p)
                attach(LEFT)
                down(HINGE_LENGTH+0.2) hirth(8,1.5,BHEIGHT/2,tooth_angle=140,base=0.3,rounding=.05,crop=true, anchor=BOTTOM);
              }
            }      
          }    
        
        // Screw + Support
        color([1,1,1])
        for (n = SCREWS) {
          left(n.x) back(n.y)
          {
            // Structure
            position(BOTTOM) {
              cyl(d=SCREWD+0.4*5,l=BHEIGHT, anchor=BOTTOM); // d Should be the same as top case
              cyl(d=SCREWHD+0.4*4,l=BHEIGHT-4, anchor=BOTTOM) {
                position(TOP) cyl(d1=SCREWHD+0.4*4,d2=4, l=1, anchor=BOTTOM);
              }
            }

            // Removals
            position(BOTTOM) down(0.01)
            tag("remove") {
              cyl(d=SCREWD+0.2,l=6*3, anchor=CENTER); 
              cyl(d=SCREWHD,l=BHEIGHT-4, anchor=BOTTOM); 
            }          
          }
        }
      }
    }
  }
}



module bottom_plate() {
  // Bottom plate
  PLATETHICK=1.5;
  KNUKLEOFFSET=0.5;
  PLATE_DOWN=($preview?0:5);
  FWD=1;
  diff()
  cuboid([HINGE_LENGTH*5+10, 45, PLATETHICK], anchor=BOTTOM, chamfer=0.2*4, edges=[TOP+LEFT, TOP+RIGHT, TOP+FRONT, TOP+BACK]) {
    
    position(TOP+BACK)     
    fwd(FWD) knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, arm_angle=90, segs=HINGE_SEGS, offset=KNUNCKLE_DIAM/2+KNUKLEOFFSET, arm_height=0, anchor=BOT+BACK, inner=false,knuckle_diam=KNUNCKLE_DIAM, pin_diam=2) {

      if (FRICTION_RING) {
       

        for (p = [HINGE_LENGTH, HINGE_LENGTH*3]) {
          RINGTHICK=2;
          right(p)
          attach(LEFT)
          tag("remove") { // 2            
            cylinder(d=BHEIGHT+1,h=2, anchor=CENTER);
            cylinder(d=BHEIGHT-RINGTHICK,h=1.5, anchor=BOTTOM) {
              position(TOP) torus(od=BHEIGHT-0.4*2,id=BHEIGHT-2*RINGTHICK, anchor=TOP);
              position(TOP) if ($preview) color([1,0.44,00,0.4]) %torus(od=BHEIGHT-0.4,id=BHEIGHT-2*RINGTHICK, anchor=TOP);
            }
          }      
        }
          
        for (p = [0, HINGE_LENGTH*2]) {
          right(p+HINGE_LENGTH)
          attach(LEFT)
          tag("remove") down(HINGE_LENGTH+1.1) xrot(180) hirth(8,1.2,BHEIGHT/2+0.4,tooth_angle=140,base=0.5,rounding=.05,crop=true,anchor=TOP);
        }
      }
    }
    
    position(TOP+BACK) tag("remove") fwd(KNUNCKLE_DIAM+2) cube([3.5, 10, 4], anchor=BACK);
    position(TOP+FRONT) back(5) cube([HINGE_LENGTH*5,5,KNUKLEOFFSET], anchor=BOTTOM);
    position(BOTTOM+BACK) cuboid([HINGE_LENGTH*5,FWD,4.4], anchor=BOTTOM+BACK, rounding=0.4, edges=[BACK+LEFT, BACK+RIGHT, BACK+TOP, TOP+LEFT, TOP+RIGHT]);
  }
}




  // Hinge 
//  LENGTH=DIAMETER/2+HINGE_ATTACH_LENGTH;
//  WIDTH=14;
//  down(BHEIGHT+PLATE_DOWN+KNUCKLE_OFFSET+($preview?3.5:15))
//  xrot(($preview?-HINGE_ANGLE:0))
//  left(HINGE_LENGTH*3-HINGE_LENGTH/2) back(3) 
//  xrot(90) 
//  zrot(-90) 
//  diff()
//  prismoid(size1=[KNUNCKLE_DIAM/8,LENGTH], size2=[KNUNCKLE_DIAM,LENGTH], shift=[(KNUNCKLE_DIAM - KNUNCKLE_DIAM/8)/2,0], h=WIDTH, anchor=FRONT+TOP) {
//       
//       
//      position(TOP+FRONT) knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, segs=HINGE_SEGS, inner=false, arm_angle=90, anchor=BOT+RIGHT, orient=UP, spin=-90,knuckle_diam=KNUNCKLE_DIAM, offset=KNUNCKLE_DIAM/2) {
//
//          color("green") attach(BACK) xrot(20) down(1) cuboid([HINGE_LENGTH-1,2.5,2], anchor=BOTTOM);
//        
//          if (FRICTION_RING) {
//            tag("remove") right(HINGE_LENGTH) attach(LEFT) {
//              color("orange") %torus(od=6,id=2, anchor=BOTTOM);
//              cylinder(d=7,h=1.75);
//            }
//            
//          }
//
//        }
//
//        position(TOP+FRONT) knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, segs=HINGE_SEGS, offset=KNUNCKLE_DIAM/2, inner=false, arm_angle=90, anchor=BOT+RIGHT, orient=UP, spin=-90, knuckle_clearance=1,knuckle_diam=KNUNCKLE_DIAM);
//
//
//      ATTACH_WIDTH=WIDTH+8;
//      echo("With of attachment: ", ATTACH_WIDTH);
//      // #position(LEFT+BACK) cube([10,WALL,WIDTH], anchor=RIGHT+BACK); on top..
//      color("lightblue")
//      position(LEFT+BACK) cube([12,3,ATTACH_WIDTH], anchor=LEFT+BACK) {
//        right(WALL) 
//        position(LEFT+FRONT) 
//        prismoid(size1=[WIDTH/2,WALL], size2=[0,WALL], shift=[-WIDTH/4,0], h=WIDTH/2, anchor=BOTTOM+LEFT, orient=FRONT);
//        
//        tag("remove") {
//          up(4) right(2) attach(FRONT) cyl(d=3, l=10) {
//            if ($preview) tag("keep") #position(TOP) cyl(d=6, l=2);
//          }
//          down(4) right(-1) attach(FRONT) cyl(d=3, l=10) {
//            if ($preview) tag("keep") #position(TOP) cyl(d=6, l=2);
//          }
//        }
//      }      
//   }
//}

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
module rrect(rt=4, rs=2, l=10, rect=[10,10], anchor=CENTER, spin=0, orient=UP) {
//  r=d/2;
  RT = rt - 0.1;
  RS = rs - 0.1;
  asize=[rect.x,rect.y,l];
  attachable(anchor, spin, orient, asize) {
    down(l/2)     offset_sweep(round_corners(rect(rect), radius=RS), height=l, top=os_circle(r=RT));
    children();
  }
}


// left(40) screwStuff(1) left(5) screwStuff(2)  left(5) screwStuff(3);

module screwStuff(struct=0, shaftl=8, shaftd=2.4, headl=2, headd=5, sunk=false, anchor=CENTER, spin=0, orient=UP) {
  WALL=2;
  
  attachable(anchor, spin, orient, r=shaftd, l=shaftl) {
  
    union() {
      // Example screw
      if (struct==1) {
        color([0.9,0.9,1,0.5]) %up(shaftl/2) left(1) screw(M2p5_pan_screw, shaftl);
      }

      // removeal of material
      if (struct==2) {
        cyl(d=shaftd,l=shaftl, anchor=CENTER) {
          position(TOP) cyl(d=headd, l=headl, anchor=BOTTOM); 
        }
      }
      
      // support
      if (struct==3) { //+(0.4*4*2)
        cyl(d=headd,l=shaftl, anchor=CENTER);
      }
    }
    children();
  }
  
  }

