# clock-driver

General info about project
modifying the motor

## 0.&nbsp;&nbsp; The Circuit
schematic
parts list

## 1.&nbsp;&nbsp; Obtaining the PCB
how to get it manufactured
Gerber files

## 2.&nbsp;&nbsp; Soldering the PCB

### Step 1.1
![image1](media/_MG_2800r.jpg)
Solder the DS3234 clock chip on underside of the PCB. This is fairly easy to do with a fine-tipped [soldering iron and desoldering braid](https://www.sparkfun.com/tutorials/96). No dedicated SMD soldering equipment is necessary.

### Step 1.2
![image1](media/_MG_2801r.jpg)
Solder male headers for the RP2040 Zero and voltage regulator. Optional: solder a female header for the motor. If you are making a Mystery Clock or Hollow Clock 4 Remix, *do not* solder a connector for the motor. Instead, the motor wires will need to be soldered directly to the PCB in order to fit in the clock base.

### Step 1.3
![image1](media/_MG_2802r.jpg)
Solder the remaining parts onto the PCB.

## 3.&nbsp;&nbsp; Converting the Stepper Motor from Unipolar to Bipolar

### Step 3.1
![image1](media/_MG_3632r.jpg)
With a utility knife, score the small tab on the plastic motor cover until it breaks off. Pry open the cover.

### Step 3.2
![image1](media/_MG_3634r.jpg)
Remove the small plastic tab that was broken off in Step 3.1.

### Step 3.3
![image1](media/_MG_3637r.jpg)
With a utility knife, cut/scrape the center PCB trace. The middle (red) wire can also be clipped off. Note the wire color corresponding to coil connections.

### Step 3.4
![image1](media/_MG_3638r.jpg)
Glue the plastic cover back on the motor. If necessary, trim the remaining tabs so that the cover can be inserted easily.
