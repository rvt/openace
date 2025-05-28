include <BOSL2/std.scad>
include <BOSL2/rounding.scad>
include <BOSL2/joiners.scad>
include <BOSL2/threading.scad>
include <NopSCADlib/core.scad>
include <NopSCADlib/vitamins/green_terminals.scad>
include <NopSCADlib/vitamins/pcbs.scad>
include <NopSCADlib/vitamins/batteries.scad>
include <NopSCADlib/vitamins/toggles.scad>
include <pcb_fixtures.scad>

$fn = $preview ? 32 : 96;


SMA_ANTENNA_HOLE_DIAM=7;
TOLERANCE=0.15; // Added or remove from parts that are designed to cut holes.

BITS=[1,2,4,8,16,32,64,128];

function bitwise_and
(
  v1,
  v2,
  bv = 1
) = ((v1 + v2) == 0) ? 0
  : (((v1 % 2) > 0) && ((v2 % 2) > 0)) ?
    bitwise_and(floor(v1/2), floor(v2/2), bv*2) + bv
  : bitwise_and(floor(v1/2), floor(v2/2), bv*2);
  // echo(bitwise_and(5,4));  
 
 
function vec2(p,d=[0,0]) = [p[0]!=undef?p[0]:d[0],p[1]!=undef?p[1]:d[1]];
function vec3(p,d=[0,0,0]) = [p[0]!=undef?p[0]:d[0],p[1]!=undef?p[1]:d[1],p[2]!=undef?p[2]:d[2]];
function vec4(p,d=[0,0,0,0]) = [p[0]!=undef?p[0]:d[0],p[1]!=undef?p[1]:d[1],p[2]!=undef?p[2]:d[2],p[3]!=undef?p[3]:d[3]];
function vec5(p,d=[0,0,0,0,0]) = [p[0]!=undef?p[0]:d[0],p[1]!=undef?p[1]:d[1],p[2]!=undef?p[2]:d[2],p[3]!=undef?p[3]:d[3],p[4]!=undef?p[4]:d[4]];
function vec6(p,d=[0,0,0,0,0,0]) = [p[0]!=undef?p[0]:d[0],p[1]!=undef?p[1]:d[1],p[2]!=undef?p[2]:d[2],p[3]!=undef?p[3]:d[3],p[4]!=undef?p[4]:d[4],p[5]!=undef?p[5]:d[5]];
function vec7(p,d=[0,0,0,0,0,0,0]) = [p[0]!=undef?p[0]:d[0],p[1]!=undef?p[1]:d[1],p[2]!=undef?p[2]:d[2],p[3]!=undef?p[3]:d[3],p[4]!=undef?p[4]:d[4],p[5]!=undef?p[5]:d[5],p[6]!=undef?p[6]:d[6]];




// side_support(bbox=false,l=10, cl=3, d=10, toPrint=1) show_anchors(std=true);

// side_support(1, l=10, cl=2,STARTSIZE=[0.01,0.1,0.1]);
// side_support(2);
// toPrint = 1 Side support over side
// toPrint = 2 Side support in corner
// toPrint = 3 Square (for lids or bottoms)
// toPrint = 4 Removal hole
// w = width
// l = length
// cl= top square size
// sideOfsset = forgot
module side_support(anchor=CENTER, spin=0, orient=UP, center, bbox=false, visual=true, toPrint=2, w=5, d=5, l=5, cl=0, screwDiam=2.2, sideOfsset=2.5, STARTSIZE=[0.01,0.01,0.01]) {
  
  o=[0,d/2-sideOfsset,0];
  ml=cl>0?l-cl:l/2;
  module support() {
    color([1,0.2,0.2]) {    
      
      if (toPrint==1) {
        hull() {
          translate([0,0,l]) cube([w,d,0.01]);
          translate([0,0,ml]) cube([w,d,0.01]);
          translate([0,0,0]) cube(STARTSIZE + [w,0,0]);
        }
      }
      if (toPrint==2) {
        hull() {
          translate([0,0,l]) cube([w,d,0.01]);
          translate([0,0,ml]) cube([w,d,0.01]);
          translate([0,0,0]) cube(STARTSIZE);
        }
      }
      if (toPrint==3) {
        hull() {
          cube([w,d,l]);
        }
      }  
    }
  }

  
  size=[w,d,l];

  anchors = [                 
    named_anchor("c", [0, 0 , size.z/2] + o, unit(UP)),
  ];  
  
  attachable(anchor, spin, orient, size, anchors=anchors) {
    union() {
      if (bbox) #cube(size, center=true);
      difference () {
        if (toPrint!=4) {
          translate(-size/2) render() support();
        }
        translate(o) cyl(d=screwDiam,l=100);
      }
    }
    children();
  }
  
}


// screw_mount(height=6);
module screw_mount(anchor=BOTTOM, spin=0, orient=UP, center, screw=2.5, height=5, d1=8) {    
  r1=d1/2;
  r2=(screw+1.5*2)/2;
  attachable(anchor, spin, orient, r1=r1, r2=r2, l=height) {
    difference() {
      cyl(h=height, d1=d1, d2=r2*2);
      up(0.5)
        cyl(h=height+2,d=screw*0.75);
    }
    children();
  }    
}


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
    
//            for ( mount = terminals ){
//              translate(mount)
//              rotate(180) green_terminal(gt_3p5, 2);
//            }
//            
//            translate(PICO) translate([0,-RPI_Pico[2]/2-2,0]) rotate(-90) pcb(RPI_Pico);
//
//          difference() {
//          cube(size, center=true);
//            for ( mount = positions ){
//              translate(mount)
//                 cyl(d=3.3,h=5);
//            }
//          }
        }
      }
      children();
    }
}


//ceramic_gps_antenna27(4,height=9,oversize=0,mountAngle=[0,0,0],flip=0);

//ceramic_gps_antenna27(1,height=9,oversize=0,mountAngle=[0,0,0],flip=0);
//  ceramic_gps_antenna27(4,height=5+(5+2),mountAngle=[0,0,0],flip=0,gpr=[45+90+180],gpd=(5+2));
// ceramic_gps_antenna27(5,height=24,mountAngle=[0,0,0]);

// ceramic_gps_antenna27(4,height=14.4);
// toPrint == 1 Print's the design
// toPrint == 2 prints the holes
// toPrint == 3 prints the plate only
// toPrint == 4 Mount (Not fully tested in all angles) oversize not used
module ceramic_gps_antenna27(toPrint=1, SCREW_DIAM=2.5, oversize=0, height=10, mountAngle=[0,0,0],flip=0,gpr=[],gpd=6) {
  HPOS=33/2;
  DIAM=38;

  // ANtenna Intself
  ant=27+oversize;
  antD2=ant/2;
  ant1=24+oversize;
  ant1D2=ant1/2;

  // Bottom Filter
  filt=26+oversize;
  filtD2=filt/2;   
    
  module mountHoles(holeDiam, height) {
    translate([HPOS,0,0])  cylinder(d=holeDiam*0.75, h=height,center=true);
    translate([0,HPOS,0])  cylinder(d=holeDiam*0.75, h=height,center=true);
    translate([-HPOS,0,0]) cylinder(d=holeDiam*0.75, h=height,center=true);
    translate([0,-HPOS,0]) cylinder(d=holeDiam*0.75, h=height,center=true);
  }

  translate([0,0,height]) rotate(mountAngle+[180*flip,0,0]) translate([0,0,-0.8*flip]){
    if (toPrint == 3) {
        color([0,0.6,0]) 
          cylinder(d=DIAM, h=0.8);
    }  
    
    if (toPrint == 1) {
      difference() {
        color([0,0.6,0]) cylinder(d=DIAM, h=0.8);
        mountHoles(SCREW_DIAM, 4);
      }
      
      // Antenna
      color([0.8,0.7,0.6])    translate([-antD2,-antD2,0.8]) cube([ant,ant,7 + 0.8 + (oversize>0?1:0)]);
      color([0.98,0.98,0.98]) translate([-ant1D2,-ant1D2,0.8]) cube([ant1,ant1,7 + 0.9 + (oversize>0?1:0)]);
      color([0.88,0.88,0.88]) translate([3,0,0.8]) cylinder(d=1.5,h=7 + 1.2);
      color([0.88,0.88,0.88]) translate([0,3,0.8]) cylinder(d=1.5,h=7 + 1.2);
      color([0.88,0.88,0.88]) translate([0,0,0.8]) cylinder(d=1,h=7 + 0.91);

      // Filter (Bottom)
      color([0.7,0.7,0.7]) translate([-filtD2,-filtD2,-3-(oversize>0?0.8:0)]) cube([filt,filt,3 + (oversize>0?0.8:0)]);
    }

    if (toPrint == 2) {
        mountHoles(SCREW_DIAM, 18);
    }
    
    if (toPrint == 5) {
        for (i = [0:3]) {
//          rotate([0,0,45+i*90]) translate([filtD2,0,0]) rotate([0,-60,0]) cylinder(d=5,l=25,center=true);
          rotate([0,0,45+i*90]) 
          translate([filtD2,0,0]) 
          rotate([0,-45,0]) 
          translate([5,0,0]) 
          cube([10,4,30],center=true);
        }
    }
  }

  if (toPrint==4) {
    difference() {
      crop([100,100,100], [0,0,height], mountAngle) 
        linear_extrude(50) 
        projection(cut = false) 
        rotate(mountAngle) 
        ceramic_gps_antenna27(3,height=0, oversize=0.5, mountAngle=[0,0,0],flip=flip);
      union() {
         ceramic_gps_antenna27(1,2,oversize=1,height=height,mountAngle=mountAngle,flip=flip);
         ceramic_gps_antenna27(2,SCREW_DIAM,0,height=height,mountAngle=mountAngle,flip=flip);
         ceramic_gps_antenna27(5,SCREW_DIAM,0,height=height,mountAngle=mountAngle,flip=0);         
         for ( r = gpr ){
           rotate( [0, 0, r])
             translate([60/2+gpd/2, 0, height/2])
             difference() {
               cylinder(d=60+gpd*2,height+2,center=true);
               cylinder(d=60,height+1,center=true);
             }
        }
      }
    }
  }
}

// sma_antenna_plug_trough(bbox=false, toPrint=1) {
//  show_anchors(std=false);
// }

// SMA Wall mounted SMA antenna
// toPrint=1 print's the device, for preview
// toPrint=2 print's the hole
// toPrint=3 Print-s extra support ring on wall
// l=10 of the cylinder that remove when toPrint=2
module sma_antenna_plug_trough(anchor=CENTER, spin=0, orient=UP, center, bbox=false, toPrint=1) {

  SMA_ANTENNA_HOLE_DIAM = 6.2 + TOLERANCE;
  length=15;
  module plug() {
    color("gold")
      render()
      down(length/2) 
      union() {
        if (toPrint==1) {
          cylinder(d=SMA_ANTENNA_HOLE_DIAM,h=length);
          up(2) cylinder(d=9,h=2,$fn=6);
          #down(20) sphere(d=1);
        }
        if (toPrint==2) {
          cylinder(d=SMA_ANTENNA_HOLE_DIAM,h=length);
        }
        if (toPrint==3) {
          up(length/4)
          difference() {
            cylinder(d=12,h=0.5,center=true);
            cylinder(d=SMA_ANTENNA_HOLE_DIAM,h=0.5);
          }
        }
      }    
  }
 

  anchors = [                 
      named_anchor("c", [0, 0 , -length/2+4], unit(DOWN)),
  ];  
  
  attachable(anchor, spin, orient, d=SMA_ANTENNA_HOLE_DIAM, l=length, anchors=anchors) {
    union() {
      if (bbox) #cyl(h=length, d=SMA_ANTENNA_HOLE_DIAM, center=true);
      plug();
    }
    children();
  } 
}

// Simple cropping function
// c=Size of cropping cube
// t=Location of cropping cube
// r=Rotation angle of cropping cube
module crop(c,t,r)
  difference() {
      children();
      translate(t) rotate(r) translate([-c.x/2,-c.y/2,0]) cube(c);
  }


  
//TBATHOLDER_18650(anchor=BOTTOM, casing=1);  
  
module TBATHOLDER_18650(anchor=CENTER, spin=0, orient=UP, casing=0) {
  SIZE=[78,22,21];
  WALL=[4,4,0];
  CASING_Z=7;
  module casing() {
        down(SIZE.y/2 - 0.5)
        difference() {
          cube(size=[SIZE.x+1, SIZE.y, 6] + WALL,anchor=BOTTOM);
          union() {
            down(0.01) union() {
              cube(size=[SIZE.x+0.8, SIZE.y, CASING_Z] + [0.5, 0.5,0],anchor=BOTTOM);
              cube(size=[SIZE.x+0.8, 5, CASING_Z] + [5,0,0],anchor=BOTTOM);
            }
            for(i = [[SIZE.x/2,SIZE.y/2],[-SIZE.x/2,SIZE.y/2],[SIZE.x/2,-SIZE.y/2],[-SIZE.x/2,-SIZE.y/2]]) {
              translate([i.x,i.y])
              cube(size=[5.1,5.1,CASING_Z], anchor=BOTTOM);
            }
          }
        }
  }

  attachable(anchor=anchor, spin=spin, orient, size=SIZE) {
  
    union() {
      
      if (casing==1) {
        casing();
      } 
        else 
      {
        %casing();
        down((SIZE.z - 1)/2) 
        cube(size=[SIZE.x,SIZE.y, 1], anchor=anchor, spin=spin, orient) {
          position(LEFT+BOTTOM)  cube(size=[1,SIZE.y,SIZE.z], anchor=CENTER+BOTTOM+LEFT);      
          position(RIGHT+BOTTOM)  cube(size=[1,SIZE.y,SIZE.z], anchor=CENTER+BOTTOM+RIGHT);      
          position(FRONT+BOTTOM)  cube(size=[SIZE.x,1,SIZE.z/2], anchor=CENTER+BOTTOM+FRONT);      
          position(BACK+BOTTOM) cube(size=[SIZE.x,1,10], anchor=CENTER+BOTTOM+BACK);      
          position(BOTTOM) up(SIZE.y/2+1) yrot(90) battery(["", "", 69, 19, 13, 10,  0,   "MediumSeaGreen", [], 0, []]);
        }
      }
      //left(5) cube(size=SIZE,anchor=anchor);
    }    
    children();
  }
}
  
  
//CHARGER_TP4056(casing=0){
//  show_anchors(std=false);
//}
module CHARGER_TP4056(anchor=BOTTOM, spin=0, orient=UP, casing=0, up=1.5) {
  SIZE=[28, 17.5, 0.1];
  TOL=0.1;

  TP4056 = ["TP4056", "TP4056 Li-lon Battery charger module", 28, 17.5, 1.5, 0, 1.0, [2.4, 2.4], "#2140BE", false,
    [[1.67, 1.8], [1.67, -1.8], [-1.67, 1.8], [-1.67, -1.8], [-1.67, -4.98], [-1.67, 4.98]],
    [ [  2, 17.5 / 2, 180, "usb_C"],
      [  7, -2      ,   0, "smd_led", LED0805, "red"],
      [ 11, -2      ,   0, "smd_led", LED0805, "blue"],

    ]];
  gt_2p54 = ["gt_2p54", 3,  7,   8.8, 3,    5, 0.0,  5.0, 0,    1,    0.2, 2,   2,   2,   0,   0,    0];
    
  module casing() {
    difference() {
      cube([TP4056[2],TP4056[3],up]-[6,0,0], anchor=CENTER+BOTTOM);
      cube([TP4056[2],TP4056[3],up]-[6+4,4,-0.1], anchor=CENTER+BOTTOM);
    }
    left(TP4056[2]/2+0.8+TOL) cube([1.2,TP4056[3],up+TP4056[4]-0.5], anchor=CENTER+BOTTOM);
    right(TP4056[2]/2+1+TOL) cube([2,TP4056[3],up+TP4056[4]], anchor=CENTER+BOTTOM);
    
    fwd(TP4056[3]/2) zrot(90) pcbSupport(up,8,TP4056[4],1,2);
    back(TP4056[3]/2) zrot(-90) pcbSupport(up,8,TP4056[4],1,2);
  }

    anchors = [
      named_anchor("usb_c", [-16, 0 , 3+up], unit(LEFT)),
  ]; 
  
  attachable(anchor=anchor, spin=spin, orient, size=SIZE, anchors=anchors) {
  
  
    union() {
      
      if (casing==1) {
        casing();
      } 
        else 
      {
        color([0,0.8,1]) %casing();
        up(up) {
          pcb(TP4056);
          
                  
          right(12.4) back(5.4) up(2.5) xrot(0) green_terminal(gt_2p54, 2);
          right(12.4) fwd(5.4) up(2.5) xrot(0) green_terminal(gt_2p54, 2);
          
  //        right(12.4) back(5.4) xrot(180) green_terminal(gt_2p54, 2);
  //        right(12.4) fwd(5.4) xrot(180) green_terminal(gt_2p54, 2);
        }
      }
      //left(5) cube(size=SIZE,anchor=anchor);
    }    
    children();
  }
}
  
  


//PICOBlock() {
//  show_anchors(std=false);
//}

module PICOBlock(anchor=CENTER, spin=0, orient=UP, casing=0, offsetPico=1) {

  SIZE=  [21,52,33.5];
  SX1262=[21,52,10];
  GPS=   [21,56,10];
  
  module casing() { //left(2.54/2) 
      right(SIZE.x/2) down(11) ycopies(spacing=2.54, n=19) cube([4,0.4*3,3], anchor=TOP+RIGHT);
      right(SIZE.x/2) down(1) {
      difference() {
        union() {
          up(.75) cube([5,SIZE.y,SIZE.z]+[0,4,5.75], anchor=RIGHT);
        }
        union() {
          up(0.3) cube(SIZE+[0.5,0.5,0.2], anchor=CENTER); // large inside
          fwd(SIZE.y/2) right(0.01) up(1) cube([10,10,10], anchor=RIGHT); // gap for bigger GPS
        }
      }    
    }
  }
  
  anchors = [                 
    named_anchor("usb", [0+-offsetPico,-SIZE.y/2,14.7], unit(FWD)),
  ];

  attachable(anchor=anchor, spin=spin, orient, size=SIZE, anchors=anchors) {
    union() {
      if (casing) {
        color([0,0.8,1,0.8]) casing();
      } 
        else 
      {
      union() {
        color([0,0.8,1,0.8]) %casing();
        
        up(10) left(offsetPico) union() {
        // PICO
        import("raspberry-pi-pico-w-rp2040/RaspberryPi-PicoW-WithPins.stl", convexity=2);
        // Module 1
        down(5.1) 
        difference() {
          back(SIZE.y/2) cube(size=GPS, anchor=BACK);
          union() {
            up(1) back(SIZE.y/2+0.01) cube(size=GPS-[5,-4,0], anchor=BACK);
            up(1) fwd(SIZE.y/2) cube(size=[GPS.x+1,5,GPS.z], anchor=BACK);
          }
        }
        // Module 2
        down(5.1+10.1) 
        difference() {
          back(SIZE.y/2) cube(size=SX1262, anchor=BACK);
          back(SIZE.y/2+0.01) up(1) cube(size=SX1262-[5,-1,0], anchor=BACK);
        }
        
        // Extra pins to show spacing
        down(27) {
          right(8.5) fwd(0.2) ycopies(spacing=2.54, n=20) cube([0.5,0.5,7]);
          left(8.5) fwd(0.2) ycopies(spacing=2.54, n=20) cube([0.5,0.5,7]);
        }
        }
      }
      }
      // %cube(SIZE, anchor=anchor, spin=spin, orient);      
    }    
    children();
  }
}

// screwHead(anchor=TOP, headDiam=5, headLength=2, totalLength=8);
  
module screwHead(anchor=CENTER, spin=0, orient=UP, headDiam=5, shaftDiam=2.4, headLength=2, totalLength=20) {
  SCREW_DIAM=shaftDiam;
  SCREWHD = headDiam;

  anchors = [                 
  ];

  attachable(anchor=anchor, spin=spin, orient=orient, d=shaftDiam, l=totalLength, anchors=anchors,expose_tags=true) {
    union() {
       {
        // Structure
        position(BOTTOM) {
          cyl(d=SCREW_DIAM+0.4*5,l=totalLength, anchor=BOTTOM); // d Should be the same as top case
          cyl(d=SCREWHD+0.4*4,l=headLength+1, anchor=BOTTOM) {
            position(TOP) cyl(d1=SCREWHD+0.4*4,d2=SCREW_DIAM+0.4*5, l=1.5, anchor=BOTTOM);
          }
        }

        // Removals
        position(BOTTOM) down(0.01)
        tag("remove") {
          down(0.01) cyl(d=SCREW_DIAM+0.3,l=totalLength+0.03+10, anchor=CENTER); 
          cyl(d=SCREWHD+0.2,l=headLength, anchor=BOTTOM);
        }
      }
    }
    children();
  }
}


module screwFixture(anchor=CENTER, spin=0, orient=UP, headDiam=5, shaftDiam=2.5, headLength=10, totalLength=20, remove=0) {
  SCREW_DIAM=shaftDiam;
  SCREWHD = headDiam;

  anchors = [                 
  ];

attachable(anchor=anchor, spin=spin, orient, d=shaftDiam, l=totalLength, anchors=anchors,expose_tags=true) {
    union() {
      // Structure
      position(BOTTOM) {
        cyl(d=SCREW_DIAM+0.4*5,l=totalLength - (SCREW_DIAM+0.4*5)/2, anchor=BOTTOM) {
          position(TOP) sphere(d=SCREW_DIAM+0.4*5);
        }
      }

      // Removals
      position(BOTTOM) down(1.01)
      tag("remove")
        down(0.01) cyl(d=SCREW_DIAM,l=totalLength - (SCREW_DIAM+0.4*5)/2+0.03, anchor=BOTTOM); 
    }   
    children();
  }   
}



//up(40) electronicsBox(isize=[40,50,1], lid=1) {
//  show_anchors(std=true);
//  position(LEFT + BOTTOM) left(3) sphere(d=2);
//  position(BACK + BOTTOM) sphere(d=2);
//    position( TOP + LEFT ) left(-4) sphere(d=2);
//}
//
//electronicsBox(isize=[40,50,10], anchor="bottom", lid=0) {
//  show_anchors(std=false);
//  position(LEFT + BOTTOM) sphere(d=4);
//  position(BACK + BOTTOM) sphere(d=4);
//  position(BACK + TOP) sphere(d=2);
//}
  

   
module electronicsBox(isize, wall=2.8, bottom=3, ichamfer=2, rounding=2, anchor=BOTTOM, orient=UP, rim=1.5, lid=false, spin=0) {

  boundingBox=isize + [wall*2,wall*2,0];
  module box() {
    diff()
      cuboid(
        [boundingBox.x,boundingBox.y,bottom], rounding=rounding,
        edges=[BACK+RIGHT, BACK+LEFT,FRONT+RIGHT, FRONT+LEFT], anchor=BOTTOM
      ) {
      
        position(TOP)
          rect_tube(
            h=boundingBox.z, size=[boundingBox.x,boundingBox.y], ichamfer=ichamfer,
            wall=wall,
            rounding=rounding,anchor=BOTTOM
        ) {
          if (rim>0) {
            RIM_TOL=0.2;
            position(TOP)
            if (lid) {
              // TODO: MINOR calculate correct rounding
              rect_tube(h=rim, size=[boundingBox.x,boundingBox.y]-[wall+RIM_TOL*2, wall+RIM_TOL*2], ichamfer=ichamfer, wall=wall/2-RIM_TOL, rounding=rounding,anchor=BOTTOM);
            } else {
              rect_tube(h=rim, size=[boundingBox.x,boundingBox.y] , wall=wall/2-RIM_TOL, rounding=rounding,anchor=BOTTOM);

            }
          }
        }
      }
  }
  
  anchors = custom_anchors(isize+[wall*2,wall*2,bottom], lid?bottom/2:-bottom/2);
  
  attachable(anchor=anchor, spin=spin, orient=orient, size=isize, anchors=anchors) {
    union() {
      if (lid) {
        zflip() yflip() down(isize.z/2+bottom) box();
      } else {
        down(isize.z/2+bottom) box();
      }
    }    
    children();
  }
  
}  



// Generate all 24 anchors (faces + edges + corners)
function custom_anchors(size, ofs=0) = concat(
    // 6 faces
    [
        named_anchor("left",   [-size.x/2,     0,          ofs], LEFT),
        named_anchor("right",  [ size.x/2,     0,          ofs], RIGHT),
        named_anchor("front",  [     0,   -size.y/2,       ofs], FWD),
        named_anchor("back",   [     0,    size.y/2,       ofs], BACK),
        named_anchor("top",    [     0,        0,    size.z/2+ofs], UP),
        named_anchor("bottom", [     0,        0,   -size.z/2+ofs], DOWN)
    ],

    // 12 edge centers
    [
        // Along X edges
        named_anchor("front+top",   [0, -size.y/2,  size.z/2+ofs], FWD),
        named_anchor("front+bottom",[0, -size.y/2, -size.z/2+ofs], FWD),
        named_anchor("back+top",    [0,  size.y/2,  size.z/2+ofs], BACK),
        named_anchor("back+bottom", [0,  size.y/2, -size.z/2+ofs], BACK),

        // Along Y edges
        named_anchor("left+top",    [-size.x/2, 0,  size.z/2+ofs], LEFT),
        named_anchor("left+bottom", [-size.x/2, 0, -size.z/2+ofs], LEFT),
        named_anchor("right+top",   [ size.x/2, 0,  size.z/2+ofs], RIGHT),
        named_anchor("right+bottom",[ size.x/2, 0, -size.z/2+ofs], RIGHT),

        // Along Z edges
        named_anchor("front+left",  [-size.x/2, -size.y/2, 0+ofs], FWD),
        named_anchor("front+right", [ size.x/2, -size.y/2, 0+ofs], FWD),
        named_anchor("back+left",   [-size.x/2,  size.y/2, 0+ofs], BACK),
        named_anchor("back+right",  [ size.x/2,  size.y/2, 0+ofs], BACK)
    ],

    // 8 corners
    [
        for (x = [["left", -1], ["right", 1]])
        for (y = [["front", -1], ["back", 1]])
        for (z = [["bottom", -1], ["top", 1]])
            named_anchor(
                str(x[0], "+", y[0], "+", z[0]),
                [x[1]*size.x/2, y[1]*size.y/2, z[1]*size.z/2+ofs],
                z[0] == "top" ? UP : DOWN
            )
    ]
);
    
  

module TestPartsPicoBLickCharger() {
diff()
  cube([80,45,2], anchor=BOTTOM) {
    tag("remove") position(BOTTOM) cube([680,45,20], anchor=TOP);

    position(BACK+LEFT) right(5) fwd(5) yrot(180) screwFixture();
    right(10) position(BACK+LEFT+TOP) cube([10,10,10], anchor=BOTTOM+LEFT+BACK) {
      position(TOP) screwHead(anchor=BOTTOM, orient=DOWN);
    }
    
    right(-3) position(TOP+RIGHT)  PICOBlock(anchor=RIGHT+FRONT, orient=BACK, spin=90, casing=1);
    position(TOP+LEFT) right(3) fwd(5) zrot(90)  CHARGER_TP4056(anchor=BOTTOM+BACK, casing=1);
  }
}
