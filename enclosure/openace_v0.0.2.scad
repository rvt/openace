include <BOSL2/std.scad>
include <BOSL2/rounding.scad>
include <BOSL2/walls.scad>
include <parts.scad>
include <BOSL2/hinges.scad>
include <NopSCADlib/vitamins/batteries.scad>
include <NopSCADlib/vitamins/toggles.scad>


// Change log
// 9 Oct 2024 set chamfer of box to 0
// 9 Oct 2024 widen USB plugs


$fn = $preview ? 20 : 76;
BOX_CORNER_RADUIS=4; // [0:5]

VISUALIZE_PCB=true;
VISUALIZE_TEXT=true;
VISUALIZE_LID_ON_TOP=true;
VISUALIZE_MAIN_CASE=true;

// Width of all the walls of the box
WALL_THICKNESS = 2.4; // [1.6, 2.0, 2.4, 2.8]
// Thickness of the bottom of the caseinclude 
BOTTOM_THICKNESS = 2.5; // [2.0, 2.5, 3, 3.5]
TOP_THICKNESS = 2.0; // [2.0, 2.5, 3, 3.5]

// Add a hinge for a smaller GPS on LID
HINGE_ON_LID=true;

// Printer tolerance (Mainly used to fix the box)
PRINT_TOLERANCE=0.2; // [0.05, 0.075, 0.1, 0.15, 0.175, 0.2]
// Screw diameter if the screws for the pi
PCB_SCREW_DIAM=2.5; // [2:2.2,2.4,2.5,3.0]
// Screw diameter for the lid
LID_SCREW_DIAM=3; // [2:2.2,2.4,2.5,3.0]
// Rim lid
RIM=0.75; // RVT Decreased by 0.75

VISUALIZE_CUT=true;
VISUALIZE_CUT_POS=75; // [-30,-21,0,47,75]

BATTERY_COMPARTEMENT_WIDTH=22;

/* [Hidden] */
LID_HOLE_DIAM=LID_SCREW_DIAM+0.5;
LID_INNER_HIGHT=2;
ATTACH_OFFSET = 0.01; // BOLS2 seems to leave part deatched.

CASE_HEIGHT= 15;

case_inner=[90+BATTERY_COMPARTEMENT_WIDTH, 87];
case=case_inner + [WALL_THICKNESS*2, WALL_THICKNESS*2];
echo("Case height:", CASE_HEIGHT);
echo("CASE", case);

FEET_HEIGHT=4; //RVT INCREASED BY ONE

BAT_HOLDER_HEIGHT=2;
BAT_HOLDER=[BATTERY_COMPARTEMENT_WIDTH+WALL_THICKNESS*2, case.y];
//down(BAT_HOLDER_HEIGHT+2) left(case.x/2-BAT_HOLDER.x/2)
back(100) left(40)
bottomAndWall(size=BAT_HOLDER, wall=WALL_THICKNESS, bottom=2, rounding=BOX_CORNER_RADUIS, ichamfer=0, h=BAT_HOLDER_HEIGHT) {

  position(BACK)
    cuboid([16,2,12], anchor=FRONT+BOTTOM, rounding=2, edges=[BACK+BOTTOM,RIGHT+BACK,LEFT+BACK,TOP+BACK]);
  position(FRONT)
    cuboid([16,2,12], anchor=BACK+BOTTOM, rounding=2, edges=[FRONT+BOTTOM,RIGHT+FRONT,LEFT+FRONT,TOP+FRONT]);
}


// up(25) right(45) fwd(72) zrot(90) import("gps-antenna-holder.stl", convexity=2);


//////////////// CASE ////////////////
removeTag=$preview?"preview":"remove";
if (VISUALIZE_MAIN_CASE)
left_half(x=VISUALIZE_CUT&&$preview?VISUALIZE_CUT_POS:1000,s=200)
diff("remove", "keep")
color_this("gray")
up(ATTACH_OFFSET)
bottomAndWall(size=case, wall=WALL_THICKNESS, bottom=BOTTOM_THICKNESS, rounding=BOX_CORNER_RADUIS, ichamfer=0, h=CASE_HEIGHT, rim=RIM)
{   
    position("iright") 
      left(0.3)
      up(FEET_HEIGHT) 
    
      color_this("green")
      openace_pcb(anchor=RIGHT+BOTTOM, visual=$preview && VISUALIZE_PCB) {
        // Screw Mounts PI
        attach(["mnt1", "mnt2", "mnt3", "mnt4"]) 
          color("lightgreen") orient(anchor=BOTTOM) down(ATTACH_OFFSET) {
              screw_mount(anchor=TOP,height=FEET_HEIGHT, screw=PCB_SCREW_DIAM);
              tag("remove") cyl(d=PCB_SCREW_DIAM*0.75,l=FEET_HEIGHT+BOTTOM_THICKNESS-0.5, anchor=TOP);
            }

              
        //
        tag("remove") {
          left(0.4)
          attach(["ant1","ant2","adsb", "gps"]) {
            cube([7,7,WALL_THICKNESS+0.2],anchor=BOTTOM);
          }
          attach("PICO") {
             {
              cuboid([11.5,6.5,5], rounding=2, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK]);
            }
          }            
          attach("POWER") {
             {
              cuboid([12,7,5], rounding=2, anchor=BOTTOM, edges=[LEFT+FWD, RIGHT+FWD, LEFT+BACK, RIGHT+BACK]);
            }
          }  
          attach("FLASH") {
             {
              cuboid([14,2,5], anchor=BOTTOM);
            }
          }  
        }
      }      

    // Switch    
      MINI_ROCKER = ["", "", 5, 8, 8.89,      // 0
        0.4, 2.0, "red", 5.1, 4,   // 5
        6.5, 0, 0, 1.45,          // 10
        25/2, 10.67, 2.92, 0,   ["M4_nut",           4,   8.1, 1.2, 5,    M4_washer,     M4_nut_trap_depth, 0, [8,   5.74]],  // 15
        M4_washer, 3, [3.94, 2.03, 0.76, 3, 4.83]]; // 20
 
    if ($preview)
     position("iback") left(20) up(8.5) yrot(90) xrot(-90) toggle(MINI_ROCKER, 3);  
      
      
    // Battery compartement    removal 
    tag("remove")  
      position(BOTTOM+LEFT) 
        down(0.01)
        right(WALL_THICKNESS) 
        cube([BATTERY_COMPARTEMENT_WIDTH,case_inner.y,BOTTOM_THICKNESS+0.4], anchor=LEFT+BOTTOM);
      
    // battery / PCB seperation
    position("ileft") 
      right(BATTERY_COMPARTEMENT_WIDTH) 
      cube([2,case_inner.y,CASE_HEIGHT-8], anchor=LEFT+BOTTOM) {
        tag("remove")
          up(0.01) back(32) align(FRONT,TOP)
          cube([2.2,25,8], anchor=RIGHT);
        
      }


    // battery
    bcontact = ["bcontact", 9.33, 9.75, 0.4, 2.86, 6, [1.6, 3, 5], [4.5, batt_spring]];
    if ($preview)
      position("ileft") 
      right(BATTERY_COMPARTEMENT_WIDTH/2) up(3)
      xrot(90) 
        battery(["", "",69, 19, 13, 10,  0,   "MediumSeaGreen", [], 0, []]);
    
            
    // Mount for Lid
    down(RIM)  // 1.25 for RIM
      mount_system(h=9);
 
      
    // Extra material for support      , center thingy
    position("inside") 
      down(ATTACH_OFFSET) 
      cube([30,30,1],anchor=BOTTOM) {
      tag("remove") {
        position(TOP) {
          grid_copies(spacing=20, size=20)      
            up(0.4) cylinder(d1=2.5, d2=5, h=1, anchor=TOP);        
          ycopies(12,3) {
            up(0.4) cylinder(d1=2.5, d2=5, h=1, anchor=TOP);   
          }
        }
      }
    }
      
    // Screw holes for mount
    tag("remove") 
    position(BOTTOM) {
      grid_copies(spacing=20, size=20) {
        up(0.4) cylinder(d=2.5, h=BOTTOM_THICKNESS+1, anchor=BOTTOM);
      }

      ycopies(12,3) {
        up(0.4) cylinder(d=2.5, h=BOTTOM_THICKNESS+1, anchor=BOTTOM);
      }
    }
}


//////////////// LID ////////////////

if (true)
left($preview && VISUALIZE_LID_ON_TOP?0:case_inner.x+10) 
up($preview && VISUALIZE_LID_ON_TOP?CASE_HEIGHT+1+TOP_THICKNESS+0.5:TOP_THICKNESS+LID_INNER_HIGHT)
left_half(x=VISUALIZE_CUT&&$preview?VISUALIZE_CUT_POS:1000,s=200)
diff("removeBox")
  yrot(VISUALIZE_LID_ON_TOP?0:180)
  color_this("gray")
  bottomAndWall(size=case, wall=WALL_THICKNESS, bottom=TOP_THICKNESS, rounding=BOX_CORNER_RADUIS, ichamfer=3, h=-LID_INNER_HIGHT, rim=RIM) {
     //#show_anchors(std=false);

     tag("removeBox")
       down(0.4) 
       position(TOP) 
       top_text();
     
     position(BOTTOM) 
       mount_system(hole=false, h=LID_INNER_HIGHT, type=0);
     
     tag("removeBox")
       mount_system(hole=true, type=0);
        
        
      if (HINGE_ON_LID) {
        KNUNCKLE_DIAM=6;       // Size of the nuckle    
        HINGE_LENGTH=6;        // Length of each hinge  
        KNUCKLE_OFFSET = 1;
        HINGE_SEGS=5;
//        back(HINGE_SEGS*3)
        position(RIGHT+FRONT+TOP)
        cuboid([HINGE_LENGTH*5+0, 7, 0.2*3], anchor=FRONT+TOP+LEFT, orient=LEFT, spin=90, chamfer=0.2*3, edges=[BOTTOM+BACK]) {
          left(HINGE_LENGTH*3-HINGE_LENGTH/2) 
          attach(BOTTOM)
          down(0.5)
          position(FRONT+TOP) 
            knuckle_hinge(length=HINGE_LENGTH*HINGE_SEGS, arm_angle=90, segs=HINGE_SEGS, offset=KNUNCKLE_DIAM/2+KNUCKLE_OFFSET, 
            arm_height=0, anchor=BOT+RIGHT+FRONT, inner=false, knuckle_diam=KNUNCKLE_DIAM);     
        }
      }

  }


module mount_system(hole=false, h=10, type=1) {
    OFFSET_RIGHT=5;
  // FOr case
  if (type==1) {
    position(TOP+LEFT+BACK) translate([WALL_THICKNESS,-WALL_THICKNESS]) zrot(-90) 
      side_support(d=5, w=5, cl=5, l=h, toPrint=hole?4:2, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_SCREW_DIAM);    
    position(TOP+LEFT+FRONT) translate([WALL_THICKNESS,WALL_THICKNESS]) zrot(0) 
      side_support(d=5, w=5, cl=5,  l=h, toPrint=hole?4:2, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_SCREW_DIAM);    
    
    position(TOP+RIGHT+FRONT) translate([-WALL_THICKNESS-OFFSET_RIGHT-6,WALL_THICKNESS]) zrot(0) 
      side_support(d=4, w=6, cl=4,  l=h, toPrint=hole?4:1, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_SCREW_DIAM);    
    
    position(TOP+RIGHT+BACK) translate([-WALL_THICKNESS-OFFSET_RIGHT,-WALL_THICKNESS]) zrot(180) 
      side_support(d=4, w=6, cl=4,  l=h, toPrint=hole?4:1, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_SCREW_DIAM); 

  }
   
  // fot LID 
  if (type==0) {
    position(TOP+LEFT+BACK) translate([WALL_THICKNESS,-WALL_THICKNESS]) zrot(-90) 
      side_support(d=5, w=5, l=h, toPrint=hole?4:3, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_HOLE_DIAM);    
    position(TOP+LEFT+FRONT)  translate([WALL_THICKNESS,WALL_THICKNESS]) zrot(0) 
      side_support(d=5, w=5, l=h, toPrint=hole?4:3, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_HOLE_DIAM);    
    position(TOP+RIGHT+FRONT)  translate([-WALL_THICKNESS-OFFSET_RIGHT-6,WALL_THICKNESS]) zrot(0)
      side_support(d=4, w=6, l=h, toPrint=hole?4:3, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_HOLE_DIAM);    
    position(TOP+RIGHT+BACK) translate([-WALL_THICKNESS-OFFSET_RIGHT,-WALL_THICKNESS]) zrot(180) 
      side_support(d=4, w=6, l=h, toPrint=hole?4:3, anchor=FRONT+LEFT+TOP, sideOfsset=2.5, screwDiam=LID_HOLE_DIAM); 
 
  }
}

module top_text(h=1) {
  color("darkgray") {

      linear_extrude(h) zrot(0) 
        text("OPENACE", font = "Arial Black", size = 8, anchor=CENTER);


      right(case_inner.x/2) fwd(29) linear_extrude(h) zrot(0) 
        text("GPS", halign="right", valign="center", font = "Arial Black", size = 5);


      right(case_inner.x/2) fwd(12) linear_extrude(h) zrot(0) 
        text("ADSB", halign="right", valign="center", font = "Arial Black", size = 5);

      right(case_inner.x/2) back(30) linear_extrude(h) zrot(0) 
        text("1:868", halign="right", valign="center", font = "Arial Black", size = 5);

      right(case_inner.x/2) back(10) linear_extrude(h) zrot(0) 
        text("2:868", halign="right", valign="center", font = "Arial Black", size = 5);
    }
  }

  
module bottomAndWall(h, size, wall, bottom, ichamfer=3, rounding=0, center, anchor=BOTTOM, spin=0, orient=UP, rim=0) {

  hWall=h<0?-h-rim:h-rim;
  
  hhh=h<0?-h:h;
  Sw=h<0?1:0;
  module box() {
          diff("removeSide")
            cuboid(
              vec3(size) + [0,0,bottom], rounding=rounding,
              edges=[BACK+RIGHT, BACK+LEFT,FRONT+RIGHT, FRONT+LEFT], anchor=h<0?TOP:BOTTOM
            ) {
            
            down(h<0?-ATTACH_OFFSET:ATTACH_OFFSET)
            position(h<0?BOTTOM:TOP)
              rect_tube(
                h=hWall, size=size, ichamfer=ichamfer,
                wall=wall,
                rounding=rounding,anchor=h<0?TOP:BOTTOM
            ) {
              if (rim!=0) {    
                position(h<0?BOTTOM:TOP)
                  rect_tube(
                    h=rim, size=size-[wall*Sw,wall*Sw],
                    wall=wall*0.5-0.2,
                    rounding=rounding-(h<0?wall/2:0),anchor=h<0?TOP:BOTTOM
                );  
              }
            }

            
          }
  }
  
  aSize=vec3(size)+[0,0,hhh+bottom];

  hHight=[0,0,-aSize.z/2];
  bOffset=h<0?bottom:bottom;
  

  anchors = [                 
    named_anchor("inside", hHight+[0,0,bOffset], unit(h<0?DOWN:UP)),
    named_anchor("ileft", hHight+[-size.x/2+wall,0,bOffset], unit(RIGHT)),
    named_anchor("iright", hHight+[size.x/2-wall,0,bOffset], unit(LEFT)),
    named_anchor("iback", hHight+[0,size.y/2-wall,bOffset], unit(FWD)),
    named_anchor("ifront", hHight+[0,-size.y/2+wall,bOffset], unit(BACK)),

    // Inside Front/Bottom Left/Right
    named_anchor("ibl", hHight+[-size.x/2+wall,size.y/2-wall,bOffset], unit(h<0?DOWN:UP)),
    named_anchor("ibr", hHight+[size.x/2-wall,size.y/2-wall,bOffset], unit(h<0?DOWN:UP)),
    named_anchor("ifl", hHight+[-size.x/2+wall,-size.y/2+wall,bOffset], unit(h<0?DOWN:UP)),
    named_anchor("ifr", hHight+[size.x/2-wall,-size.y/2+wall,bOffset], unit(h<0?DOWN:UP)),
    named_anchor("ifc", hHight+[0,-size.y/2+wall,bOffset], unit(h<0?DOWN:UP)),
    
    // Top Front/Bottom Left/Right
    named_anchor("tbl", hHight+[-size.x/2+wall,size.y/2-wall,aSize.z], unit(h<0?DOWN:UP)),
    named_anchor("tbr", hHight+[size.x/2-wall,size.y/2-wall,aSize.z], unit(h<0?DOWN:UP)),
    named_anchor("tfl", hHight+[-size.x/2+wall,-size.y/2+wall,aSize.z], unit(h<0?DOWN:UP)),
    named_anchor("tfr", hHight+[size.x/2-wall,-size.y/2+wall,aSize.z], unit(h<0?DOWN:UP)),
  ];  

  attachable(anchor, spin, orient, size=aSize, center, anchors=anchors) {
    union() {
      up(h<0?aSize.z/2:-aSize.z/2) box();
    }    
    children();
  }
  
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


// openace_pcb( visual=true) {
//  show_anchors(std=false);
//}
module openace_pcb(anchor=BOTTOM, spin=0, orient=UP, center, bbox=false, rough=false, visual=true, pi=4) {
    size=[86.87,84,1.6];
    corner=[-size.x/2, -size.y/2, -size.z/2];

    positions=[
        [-80.9/2, -78/2, -size.z/2],
        [-80.9/2, 78/2, -size.z/2],
        [80.9/2, 78/2, -size.z/2],
        [80.9/2, -78/2, -size.z/2]
    ];
    
    xornerOff=3.5;
    
    terminals = [
      [0,22.7,1.6] + positions[0],
      [0,15.3,1.6] + positions[0],
      [0,7.43,1.6] + positions[0]
    ];
    
    PICO = [positions[1].x+31.6,positions[1].y+xornerOff,size.z/2];
    
    anchors = [                 
        named_anchor("mnt1", positions[0], [0,0,-1]),
        named_anchor("mnt2", positions[1], [0,0,-1]),
        named_anchor("mnt3", positions[2], [0,0,-1]),
        named_anchor("mnt4", positions[3], [0,0,-1]),

        named_anchor("right", [positions[2].x+xornerOff,0,size.z/2], unit(RIGHT)),

                
        named_anchor("ant1", [positions[2].x+xornerOff,positions[2].y-8.63,size.z/2+0.2], unit(RIGHT)),
        named_anchor("ant2", [positions[2].x+xornerOff,positions[2].y-29.6,size.z/2+0.2], unit(RIGHT)),
        named_anchor("adsb", [positions[2].x+xornerOff,positions[2].y-50.67,size.z/2+0.2], unit(RIGHT)),

        named_anchor("ant1_h", [positions[2].x+xornerOff,positions[2].y-8.63,size.z/2+6.5], unit(RIGHT)),
        named_anchor("ant2_h", [positions[2].x+xornerOff,positions[2].y-29.6,size.z/2+6.5], unit(RIGHT)),
        named_anchor("adsb_h", [positions[2].x+xornerOff,positions[2].y-50.67,size.z/2+6.5], unit(RIGHT)),

        named_anchor("gps",  [positions[2].x+xornerOff,positions[2].y-69,size.z/2+4.2], unit(RIGHT)),
        named_anchor("PICO", PICO + [0,0,2.5], unit(BACK)),
        named_anchor("POWER", [positions[0].x+15.05,positions[0].y-xornerOff,size.z/2+5.0], unit(FRONT)),
        named_anchor("FLASH", [positions[0].x+47.46,positions[0].y-xornerOff,size.z/2+0.5], unit(FRONT)),

        named_anchor("SWITCH", terminals[0]+[-xornerOff,0,2.5], unit(LEFT)),
        named_anchor("BAT", terminals[1]+[-xornerOff,0,2.5], unit(LEFT)),
        named_anchor("5VIN", terminals[2]+[-xornerOff,0,2.5], unit(LEFT)),
        ];  

    attachable(anchor, spin, orient, size=size+[0,0,0], center, anchors=anchors) {
      render(3) union() {
        if (visual) {    
          down(0.9) import("ImageToStl.com_openace.stl", convexity=2);
        }
      }
      children();
    }
}


module roundRect(d=6.5,l=13,z=10) {
  render()
  union() {
    back(l/2 - d/2) cylinder(d=6.5,z);
    fwd(l/2 - d/2) cylinder(d=6.5,z);
    cube([6.5,l-d,z], anchor=BOTTOM);
  }
}

