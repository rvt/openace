// THis file contains different PCB holders for differentg PCB commonly found on the internet
//$fn=90;


//h=height of the support area (under the pcb)
//width=of the support
//circuitThick = thickness of the PCB board
//clip=0 Holder + stopper
//clip=1 Holder + clip
//clip=2 stopper
//clip=3 holder
//supportWidth=width of the support on the back
module pcbSupport(h=6,
    width=15,
    circuitThick=1.5,
    clip=1,
    supportWidth=0.4
) render(){ {
    wall=1.2; 
    pointy=2;
    backSupportAbove=0.35;
    backSupportWidth=1.0;
    backSupportLength=1;
    supportWidthTop=0.4; // Overlap top circuit board (Area that hold the circuit
    circuitThickTol=0.2; // Extra tolerance for circuit board
    b3 = clip==0?
    [
        [-wall,0],
        [-wall,h+circuitThick+circuitThickTol],
        [0,h+circuitThick+circuitThickTol],
        [0,h],
        [supportWidth,h],
        [supportWidth,0]    
    ]:clip==1?[
        [-wall,0],
        [-wall,h+pointy+circuitThick+circuitThickTol],
        [-wall + 0.8,h+pointy+circuitThick+circuitThickTol],
        [supportWidthTop,h+circuitThick+circuitThickTol+supportWidthTop],
        [supportWidthTop,h+circuitThick+circuitThickTol+supportWidthTop/2],
        [0,h+circuitThick+circuitThickTol],
        [0,h],
        [supportWidth,h],
        [supportWidth,0]    
    ]:clip==2?[
        [-wall,0],
        [-wall,h+circuitThick],
        [0,h+circuitThick],
        [0,h],
        [0,0]     
    ]:[
        [-supportWidth,0],
        [-supportWidth,h],
        [0,h],
        [0,h],
        [0,0] 
    ];
    
    color([0.5,0.5,1]) { 
        // Add 0.1 tolerance so they will fit slightly better
        translate([-0.1,width/2,0]){
            rotate([90,0,0]) 
                linear_extrude(height=width) 
                    polygon(b3);
        }
        
        if (clip==1 || clip==2) {
            // WHen adding circuitThick to h the clip doesn´t bend as good
            translate([-backSupportLength-wall,-backSupportWidth*2+width/2,0])
                cube([backSupportLength,backSupportWidth,h+backSupportAbove]);

            translate([-backSupportLength-wall,backSupportWidth-width/2,0])
                cube([backSupportLength,backSupportWidth,h+backSupportAbove]);           
        }
    }
}}



