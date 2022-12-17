// d1=83-84mm              80mm
// h1=68mm
// d2=d1 - 2*(1..1.5mm)  2x 2mm
// h2=1.3mm-1.4mm

outer_r = 36;
base_h = 2;
total_h = 50;
clearance = 0.35;
c_fa = 2;
c_fs = 1;
inner_r = 5;

module slot(a, d) {
  rotate(a, v=[0, 0, 1]) {
    translate([outer_r - d/2 - clearance, 0, 0]) {
      cylinder(h=100, r=d/2 + clearance, $fa = c_fa, $fs = c_fs);
    }
    translate([outer_r, 0, 0]) {
      cylinder(h=100, r=d/3, $fa = c_fa, $fs = c_fs);
    }
  }
}


ncoins = 5;
diams = [ 18.20, 23.20, 27.40,   23.25, 25.76 ];
angles=[
60.224460, 139.519824, 218.918725, 294.007368, 360.000000,

];
labels = [ "2", "1", "0.50", "0.20", "0.10", "0.05"];

translate([0,0,base_h]) {
  difference() {
    cylinder(h=total_h-base_h, r=outer_r, $fa=c_fa,$fs=c_fs);
    for (i = [0 : ncoins - 1]) {
      slot(angles[i], diams[i]);
    }
    translate([-1.5,-1.8, inner_r]) {
      cylinder(h=100, r=inner_r, $fa=c_fa,$fs=c_fs);
    }
    /*translate([-1.5,-1.8,3]) {
      cylinder(h=7, r1=8, r2=15.8, $fa=c_fa,$fs=c_fs);
    }
    translate([-1.5,-1.8,0]) {
      cylinder(h=3, r1=0, r2=8, $fa=c_fa,$fs=c_fs);
    }*/

    translate([-1.5,-1.8,inner_r]) {
      sphere(r=inner_r, $fa=c_fa,$fs=c_fs);
    }
  }
}

cylinder(h=base_h, r=outer_r, $fa=c_fa,$fs=c_fs);




// 5 Rp   r=8.58   d=17.15
// 10 Rp  r=9.58   d=19.15
// 20 Rp  r=10.52  d=21.05
// 50 Rp  r=9.10   d=18.20
// 1 Fr   r=11.60  d=23.20
// 2 Fr   r=13.70  d=27.40
// 5 Fr   r=15.73  d=31.45

// 2 EUR   d=25.76
// 1 EUR   d=23.25
// .50 EUR d=24.25
// .20     d=22.25
// .10     d=19.75
// .5      d=21.25
// .02     d=18.75
// .01     d=16.25
