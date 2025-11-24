include <parts.scad>

include <BOSL2/std.scad>
//include <BOSL2/rounding.scad>
//include <BOSL2/walls.scad>
include <BOSL2/screws.scad>
//include <BOSL2/hinges.scad>
include <NopSCADlib/vitamins/batteries.scad>
include <NopSCADlib/vitamins/toggles.scad>
include <NopSCADlib/vitamins/rockers.scad>
//include <NopSCADlib/vitamins/antennas.scad>


$fn = $preview ? 12 : 76;

////////////////////////////////////////////////////////////////// OK

// place the switch in the lid for better protection
TOGGLE_SWITCH="CK7101"; // ["MINI_TOGGLE_8", "CK7101"]
// Width of all the walls of the box
WALL_THICKNESS = 2.4; // [1.6, 2.0, 2.4, 2.8]
// Thickness of the bottom of the caseinclude 
BOTTOM_THICKNESS = 2.5; // [2.0, 2.5, 3, 3.5]
TOP_THICKNESS = 2.0; // [2.0, 2.5, 3, 3.5]

// Screw diameter for the lid
LID_SCREW_DIAM=2.4;    // [2:2.2,2.4,2.5,3.0]
LID_SCREW_HEAD_DIAM=5; // [5.0,5.5,6.0,6.5]

// Add Camera mount on top
TOP_MOUNT=false;
// Add Camera mount on the inside
INSIDE_MOUNT=true;

/* [VISUALISATION] */
VISUALIZE_LID_ON_TOP=true;
VISUALIZE_MAIN_CASE=true;
VISUALIZE_LID=true;
VISUALIZE_OTHERS=true;
VISUALIZE_SCHEMA=false;

VISUALIZE_LID_POSITION=0; // [0:0.5:50]

VISUALIZE_CUT=false;
VISUALIZE_X_CUT_POS=75; // [-100:1:100]
VISUALIZE_Y_CUT_POS=0; // [-100:1:100]


/* [Hidden] */
// Rim lid
RIM=1.2; 

USBC=[12,7,5];


////////////////////////////////////////////////////////////////// NOT YET OK

ATTACH_OFFSET = 0.01; // BOLS2 seems to leave part deatched.

case_inner=[168, 46, 20]; // 7380
lid_inner=[case_inner.x, case_inner.y, 2]; // 7380

echo("CASE", case_inner);

// Position X/Y relative to the back left corner
SWITCH_LID_POS=[73,-13, 0];  
// 'Cube' size (even though cylinder shape hehehe
SWITCH_LID_CUBE_SIZE=[24,20];
// Switch bore diam
SWITCH_LID_BORE=6.0;
// Z- Offset of the switch eg vertical offset
SWITCH_LID_Z_OFFSET=4.5;


MINI_TOGGLE_8 = ["", "", 5, 8, 8.89,      // 0
  0.4, 2.0, "red", 5.1, 4,   // 5
  6.5, 0, 0, 1.45,          // 10
  25/2, 10.67, 2.92, 0,   ["M4_nut",           4,   8.1, 1.2, 5,    M4_washer,     M4_nut_trap_depth, 0, [8,   5.74]],  // 15
  M4_washer, 3, [3.94, 2.03, 0.76, 3, 4.83]]; // 20


MICRO_ROCKER   = ["micro_rocker",   "Rocker Switch 10x15",            9, 13.2,  10.5, 15, 1.5,  8.2, 14,   9.3,  2.0, 0.8, 3.0, micro_spades];
        

// Plug for USB
if (VISUALIZE_OTHERS) {
  ycopies(spacing=12, n=2)
  back(50) 
  union() {
    // WALL_THICKNESS
    ADJSIZENZ = vec3(vec2(USBC));
    ADJSIZE = vec3(ADJSIZENZ) + [-0.5,-0.1, 0];
    
    cuboid(ADJSIZENZ + [3,3,1], rounding=2, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK]) {
      down(0.01) position(TOP) cuboid(ADJSIZENZ+[1,1,0.6], rounding=2, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK]) {
        position(TOP) down(0.01) cuboid(ADJSIZE + [0,0,WALL_THICKNESS+0.1], rounding=3, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD,
    LEFT+BACK, RIGHT+BACK]) {
          position(TOP) down(0.01) cuboid(ADJSIZE+[0.2,0.2,0.2], rounding=3, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK]);
          position(TOP) down(0.01) up(0.2) cuboid(ADJSIZE+[0.4,0.4,0.2], rounding=3, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK]);
        }
      }
      
    }
  }

  // Plate for green connector and charger
  back(50) left(20)
  difference() {
    cube([18,4,1.0], anchor=BOTTOM);
    union() {
      xcopies(spacing=3.8, n=2) 
        xcopies(spacing=10.5, n=2) 
        cube([1.2,1.2,4],anchor=CENTER);
    }
  }
}



//////////////// SCHEMA ////////////////

back(100)
if (VISUALIZE_SCHEMA) {
  back(80) CHARGER_TP4056(anchor="front", casing=$preview?0:1, spin=-90);
  left(60) zrot(0) yrot(-90) toggle(TOGGLE_SWITCH=="CK7101"?CK7101:MINI_TOGGLE_8, 3);

  left(120) back(60) TBATHOLDER_18650(anchor=LEFT+BOTTOM, casing=$preview?0:1);
  
  right(60) back(60)  zrot(90) import("raspberry-pi-pico-w-rp2040/RaspberryPi-PicoW-WithPins.stl", convexity=2);
}

        
//////////////// CASE ////////////////
removeTag=$preview?"preview":"remove";
if (VISUALIZE_MAIN_CASE)
left_half(x=VISUALIZE_CUT&&$preview?VISUALIZE_X_CUT_POS:1000,s=200)
back_half(y=VISUALIZE_CUT&&$preview?VISUALIZE_Y_CUT_POS:1000,s=200)
diff()
color_this("gray")
up(ATTACH_OFFSET)
electronicsBox(isize=case_inner, wall=WALL_THICKNESS, bottom=BOTTOM_THICKNESS, rounding=3, ichamfer=2, rim=RIM)
{   

    // Text to indicate which plug is which
    right(25) position(LEFT+BACK+BOTTOM) fwd(7) 
        linear_extrude(0.4) text("868Mhz", font = "Arial Black", size = 4, halign="left", anchor=CENTER);
    right(5) position(LEFT+FRONT+BOTTOM) back(7) 
        linear_extrude(0.4) text("GPS", font = "Arial Black", size = 4, halign="left", anchor=CENTER);

    BSIZE=[24,13.5,20.5];
      right(40) up(1) position("left+front+bottom")  {
        cuboid(size=BSIZE, anchor=FRONT+BOTTOM, rounding=7,edges=[BACK+TOP, BACK+BOTTOM] ) {
          tag("remove") position(CENTER+BACK) fwd(0.4*4) cuboid(size=BSIZE-[0.4*4*2,-4,2.8], anchor=BACK, rounding=7,edges=[BACK+TOP, BACK+BOTTOM]);

          back(6.5)
          position(RIGHT+FRONT)
          if ($preview) {
            tag("keep") zrot(-90) xrot(90) toggle(TOGGLE_SWITCH=="CK7101"?CK7101:MINI_TOGGLE_8, 3);
          } else {
            tag("remove") xcyl(d=SWITCH_LID_BORE,l=6,anchor=CENTER);
          }
        position(FRONT+LEFT+CENTER) up(2) xrot(90) linear_extrude(0.2) text("ON", font = "Arial Black", size = 5, halign="right", anchor=BOTTOM);
        position(FRONT+LEFT+CENTER) down(2) xrot(90) linear_extrude(0.2) text("OFF", font = "Arial Black", size = 5, halign="right", anchor=TOP);
        }
      }
      
    // Holder for Camera screw, only used this if your print bed is large enough
    if (TOP_MOUNT) {
      position("left+bottom") {
        L=8;
        W=22;
        HEIGHT=10;
        DEPTH=8;
        zrot(-180)
        union() {
           zrot(-90) union() up(0) {
            linear_extrude(height=HEIGHT)
            trapezoid(h=DEPTH, w1=W, w2=W/2, rounding=[5,5,0,0],chamfer=[0,0,0,0],$fa=1,$fs=1, anchor=FRONT);
          }

          tag("remove") 
            right(3) screw_hole("1/4-20", length=HEIGHT+1, thread=true, bevel1=true, $slop=0.05, anchor=BOTTOM);      
        }
      }
    } 

    if (INSIDE_MOUNT) {
      position("left+back+bottom") {
        HEIGHT=11;
        right(20) fwd(5.5) down(0.01)
          cyl(d=10,l=HEIGHT, anchor=BOTTOM) {
             position(TOP) sphere(d=10);
             position(TOP) tag("remove") cyl(d1=7, d2=0, h=5, anchor=BOTTOM);
             position(TOP) tag("remove") cyl(d=1,l=10, anchor=CENTER);
             position(BOTTOM) tag("remove") down(0.01) screw_hole("1/4-20", length=HEIGHT, thread=true, bevel1=true, $slop=0.05, anchor=BOTTOM);
          }
      }
     }

    // Antenna's
    left(4) up(2) {
      position(LEFT + CENTER + FRONT) back(7)  {
        if ($preview) {      
          sma_antenna_plug_trough(toPrint=1, orient=LEFT);
          // yrot(-90) antenna(ESP201_antenna);
        }
        else  
          tag("remove") cyl(d=SMA_ANTENNA_HOLE_DIAM, l=10, orient=LEFT);
      }

      position(LEFT + CENTER + BACK) fwd(7)  {
        if ($preview) {
          sma_antenna_plug_trough(toPrint=1, orient=LEFT);
          // yrot(-90) antenna(ESP201_antenna);
        }
        else  
          tag("remove") cyl(d=SMA_ANTENNA_HOLE_DIAM, l=10, orient=LEFT);
      }
    }

    // battery
    position(LEFT+BOTTOM) right(5.5) down(0.01)
      TBATHOLDER_18650(anchor=LEFT+BOTTOM, casing=$preview?0:1);

    // PICO + GPS + SX1262 block
    position(RIGHT+BOTTOM+BACK) 
    left(5) fwd(20) down(0.01)
    PICOBlock(anchor=RIGHT+FRONT, orient=BACK, spin=90, casing=$preview?false:true) {
      tag("remove") 
        attach("usb") {
          cuboid(USBC+[0,0,10], rounding=2, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK]);
        }  
    }

    // Battery Charger
    position(LEFT+BOTTOM+BACK) 
      right(97) 
      CHARGER_TP4056(anchor="front", casing=$preview?0:1, spin=-90) {
        tag("remove") 
          attach("usb_c") {
            cuboid(USBC, rounding=2, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK], spin=0);
          }  
        }
        
     // Mount for Lid
     mount_system();
      
}



//////////////// LID ////////////////

if (VISUALIZE_LID)
left($preview && VISUALIZE_LID_ON_TOP?0:case_inner.x+30) 
up($preview && VISUALIZE_LID_ON_TOP?case_inner.z+TOP_THICKNESS+VISUALIZE_LID_POSITION:TOP_THICKNESS)
left_half(x=VISUALIZE_CUT&&$preview?VISUALIZE_X_CUT_POS:1000,s=200)
back_half(y=VISUALIZE_CUT&&$preview?VISUALIZE_Y_CUT_POS:1000,s=200)
xrot($preview?0:180)
diff()
  yrot(VISUALIZE_LID_ON_TOP?0:180)
  color_this("gray")  
  electronicsBox(isize=lid_inner, wall=WALL_THICKNESS, bottom=TOP_THICKNESS, rounding=3, ichamfer=2, rim=RIM, lid=true, anchor="bottom") {
     //#show_anchors(std=false);
     tag("remove")
       down(0.4) 
       position("top") 
       top_text();
     
     mount_system(type=0);      

     up(1.01) {
      // PICO + GPS + SX1262 block
      position(RIGHT+TOP) 
      left(5) back(2)
      mirror([0,0,1])
        PICOBlock(anchor=RIGHT+FRONT, orient=BACK, spin=90, casing=true, teeth=false);

      // battery
      position(LEFT+TOP) 
        right(5)
        mirror([0,0,1]) TBATHOLDER_18650(anchor=LEFT+BOTTOM, casing=1);
     }
    
    }
  
  
  module mount_system(type=1) {
  // For case
  if (type==1) {
    down(0) {
      position(TOP+LEFT) right(1) screwFixture(orient=DOWN, anchor=BOTTOM, shaftDiam=LID_SCREW_DIAM, totalLength=12);

      position(TOP+FRONT+LEFT) back(1) right(10) screwFixture(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=BOTTOM, totalLength=12, spin=-90);
      position(TOP+BACK+LEFT) fwd(1) right(10) screwFixture(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=BOTTOM, totalLength=12, spin=90);

      position(TOP+FRONT+RIGHT) back(12) left(1) screwFixture(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=BOTTOM, totalLength=12, spin=180);
      position(TOP+BACK+RIGHT) fwd(12) left(1) screwFixture(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=BOTTOM, totalLength=12, spin=180);

      position(TOP+FRONT) back(1) left(4) screwFixture(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=BOTTOM, totalLength=12, spin=-90);
      position(TOP+BACK) fwd(1) left(4) screwFixture(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=BOTTOM, totalLength=12, spin=90);
    }
  }
   
  // Rot LID 
  if (type==0) {
      down(0) {
        position(BOTTOM+LEFT) right(1) screwHead(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=TOP, headDiam=LID_SCREW_HEAD_DIAM, headLength=2,  totalLength=lid_inner.z+TOP_THICKNESS);
                
        position(BOTTOM+FRONT+LEFT) back(1) right(10) screwHead(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=TOP, headDiam=LID_SCREW_HEAD_DIAM, headLength=2, totalLength=lid_inner.z+TOP_THICKNESS);
        position(BOTTOM+BACK+LEFT) fwd(1) right(10) screwHead(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=TOP, headDiam=LID_SCREW_HEAD_DIAM, headLength=2, totalLength=lid_inner.z+TOP_THICKNESS);

        position(BOTTOM+FRONT+RIGHT) back(12) left(1) screwHead(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=TOP, headDiam=LID_SCREW_HEAD_DIAM, headLength=2, totalLength=lid_inner.z+TOP_THICKNESS);
        position(BOTTOM+BACK+RIGHT) fwd(12) left(1) screwHead(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=TOP, headDiam=LID_SCREW_HEAD_DIAM, headLength=2, totalLength=lid_inner.z+TOP_THICKNESS);

        position(BOTTOM+FRONT) back(1) left(4) screwHead(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=TOP, headDiam=LID_SCREW_HEAD_DIAM, headLength=2, totalLength=lid_inner.z+TOP_THICKNESS);
        position(BOTTOM+BACK) fwd(1) left(4) screwHead(orient=DOWN, shaftDiam=LID_SCREW_DIAM, anchor=TOP, headDiam=LID_SCREW_HEAD_DIAM, headLength=2, totalLength=lid_inner.z+TOP_THICKNESS);
      }
  
  }
}


module top_text(h=1) {
  color("darkgray") {
      back(10)
      linear_extrude(h) zrot(0) 
        text("GA/TAS PULSE", font = "Arial Black", size = 8, anchor=CENTER);

      left(case_inner.x/2-2) fwd(15) linear_extrude(h) zrot(0) 
        text("GPS", halign="left", valign="center", font = "Arial Black", size = 5);

      left(case_inner.x/2-2) back(15) linear_extrude(h) zrot(0) 
        text("868", halign="left", valign="center", font = "Arial Black", size = 5);
    }
  }

 

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


module roundRect(d=6.5,l=13,z=10) {
  render()
  union() {
    back(l/2 - d/2) cylinder(d=6.5,z);
    fwd(l/2 - d/2) cylinder(d=6.5,z);
    cube([6.5,l-d,z], anchor=BOTTOM);
  }
}

