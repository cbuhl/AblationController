#include <Arduino.h>
#include "A4988.h"
#include <SerialCommand.h>
#include <Ticker.h>
#include <Wire.h> 
#include <LiquidCrystal.h>



void display_update();
void Xhome();
void Yhome();
void moveX(long displacement);
void moveY(long displacement);
void moveToX(long position);
void moveToY(long position);
void joystickStop();
void joystickMove();
int scaleJoystickPosition(int newspeed);
void enableJoystick();
void disableJoystick();
void changeLine();
void scanLine();
void parseScanCommand();
void parseMoveCommand();
void parseMoveToCommand();
void returnPosition();
void homeAllAxis();
void unrecognized(const char *command);
void setResolution();
void setStepSize();
void setCenter();
void openShutter();
void closeShutter();





// using a 200-step motor (most common)
#define MOTOR_STEPS 200
#define STANDARD_SPEED 100
#define FAST_SPEED 400
#define STANDARD_ACCEL 1000
// configure the pins connected
#define XDIR 5
#define XSTEP 6
#define YDIR 3
#define YSTEP 2
#define XEND 7
#define YEND 4
#define XJOYSTICK A0
#define YJOYSTICK A1
#define SHUTTER_PIN A5
#define RESOLUTION 256
#define SAFEDISTANCE 500 //safe distance in steps, from the edge of the relevant field (0,0).

//RS, ENABLE, D1, D2, D3, D4
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);
A4988 X(MOTOR_STEPS, XDIR, XSTEP);
A4988 Y(MOTOR_STEPS, YDIR, YSTEP);
SerialCommand sCmd;     // The SerialCommand object for parsing arguments from Serial.
//Ticker SerialPrintInfo(print_update,  1000);
Ticker DisplayPrint(display_update, 500);
//Ticker CheckJoystick(joystickMove, 1, 0, 2);

// Define relevant states:
bool xHomed = false;
bool yHomed = false;
bool manualMode = true;
bool scanMode = false;
bool safePosition = false;
bool shutterSafe = false;

//define storage array for scan lines
int scanLineContainer[RESOLUTION];
long stepsize = 200; //The stepsize per pixel
long xpos, ypos;


void setup() {
    // Define relevant callback functions for the command structure
    sCmd.addCommand("MAN",      enableJoystick);
    sCmd.addCommand("REM",      disableJoystick);
    sCmd.addCommand("SCAN",     parseScanCommand);
    sCmd.addCommand("STEP",     setStepSize);
    sCmd.addCommand("MOVE",     parseMoveCommand);
    sCmd.addCommand("MOVETO",   parseMoveToCommand);
    sCmd.addCommand("GETPOS",   returnPosition);
    sCmd.addCommand("HOME",     homeAllAxis);
    sCmd.addCommand("HOMEX",    Xhome);
    sCmd.addCommand("HOMEY",    Yhome);
    sCmd.addCommand("CENTER",     setCenter);
    sCmd.addCommand("OPEN",     openShutter);
    sCmd.addCommand("CLOSE",    closeShutter);
    sCmd.setDefaultHandler(unrecognized);


    lcd.begin(24,2);
    lcd.clear();
    DisplayPrint.start();
    //CheckJoystick.start();
    pinMode(XEND, INPUT_PULLUP);
    pinMode(YEND, INPUT_PULLUP);
    pinMode(SHUTTER_PIN, OUTPUT);


    // Set target motor RPM to 1RPM and microstepping to 16 (full step mode)
    Serial.begin(115200);
    Serial.println("Ready");
    lcd.setCursor(3, 1);
    lcd.print("Ablation Controller");

    X.begin(STANDARD_SPEED, 16);
    Y.begin(STANDARD_SPEED, 16);
    X.setSpeedProfile(X.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
    Y.setSpeedProfile(Y.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);

    closeShutter();

    X.enable();
    Y.enable();

    if (!xHomed){
        //Xhome();
    }
    
    if (!yHomed){
        //Yhome();
    }
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////


void loop() {

    joystickMove();
    //SerialPrintInfo.update();
    DisplayPrint.update();
    sCmd.readSerial();
    
    
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////



void Xhome() {
    X.setRPM(300);
    X.setSpeedProfile(X.CONSTANT_SPEED);
    while (!digitalRead(XEND))
    {
        X.move(10);
    }
    X.stop();
    X.setSpeedProfile(X.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
    X.setRPM(STANDARD_SPEED);
    xHomed = true;
    
}

void Yhome() {
    Y.setRPM(300);
    Y.setSpeedProfile(Y.CONSTANT_SPEED);
    while (!digitalRead(YEND))
    {
        Y.move(10);
    }
    Y.stop();
    Y.setSpeedProfile(Y.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
    Y.setRPM(STANDARD_SPEED);
    yHomed = true;
    
}

void joystickMove() {
    if (manualMode) {
        DisplayPrint.pause();
        bool xDead = true;
        bool yDead = true;

        int xJoyPos = analogRead(XJOYSTICK);
        int yJoyPos = analogRead(YJOYSTICK);

        // check if the position is outside the deadzone:
        if ((480 > xJoyPos) or (xJoyPos > 550)) {
            xDead = false;
        }
        if ((480 > yJoyPos) or (yJoyPos > 555)) {
            yDead = false;
        }

        if (!xDead) {
            int newspeed = xJoyPos-506;
            int sign = newspeed/abs(newspeed);

            X.setRPM(scaleJoystickPosition(newspeed));
            //Serial.println(X.getRPM());
            X.setSpeedProfile(X.CONSTANT_SPEED);
            X.move(sign * X.getRPM());
        }
        
        if (!yDead) {
            int newspeed = yJoyPos-511;
            int sign = newspeed/abs(newspeed);

            Y.setRPM(scaleJoystickPosition(newspeed));
            //Serial.println(Y.getRPM());
            Y.setSpeedProfile(Y.CONSTANT_SPEED);
            Y.move(sign * Y.getRPM());
        }

        //X.nextAction();
        //Y.nextAction();


        X.setRPM(STANDARD_SPEED);
        X.setSpeedProfile(Y.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
        DisplayPrint.resume();


    }
}


int scaleJoystickPosition(int newspeed) {
    // This scaling function ensure that the user has a sane range and acceleration on the joystick. 
    // It enables you to do both fine adjustments, as well as large moves, with a bit of practice.
    float intermediate;
    float output;

    intermediate = (float(abs(newspeed)) - 21.0)/19.0;
    output = exp(intermediate);
    
    return int(output);

}


void display_update() {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("X pos: ");
    lcd.setCursor(0,1);
    lcd.print("Y pos: ");

    lcd.setCursor(7,0);
    lcd.print(xpos);

    lcd.setCursor(7,1);
    lcd.print(ypos);

    // Reporting the joystick status
    lcd.setCursor(17, 0);
    if (manualMode) {
        lcd.print("MANUAL");
    }
    else {
        lcd.print("REMOTE");
    }
    // Reporting Shutter laser status.
    lcd.setCursor(17,1);
    if (shutterSafe) {
        lcd.print("SAFE");
    }
    else {
        lcd.print("UNSAFE");
    }
}


void enableJoystick() {
    manualMode = true;
    scanMode = false;
    Serial.println("RETURN: Joystick enabled");
}

void disableJoystick() {
    manualMode = false;
    scanMode = true;
    Serial.println("RETURN: Joystick disabled");
}



void changeLine() {
    closeShutter();
    //Check to see if we are back to the zeroposition, or get us there.
    if (xpos != -SAFEDISTANCE) {
        moveToX(-SAFEDISTANCE);
    }
    //increase the stepsize to the right position.
    moveY(stepsize);

    

}

void scanLine(){
    //move to safe position, first line, 0 (backlash correction), for the first line scan.
    if (safePosition) {
        moveToY(0);
        safePosition = false;
    }
    //scan the line
    X.setSpeedProfile(X.CONSTANT_SPEED);
    moveToX(0);
    openShutter();
    //delay before start
    delay(100);
    for (int i = 0; i < RESOLUTION; i++)
    {
        int rpm = scanLineContainer[i];
        //sanity check on the scan speed
        if (rpm <= 1){
          rpm = 1;
        }
        if (rpm >= 300){
          rpm = 300;
        }
        X.setRPM(rpm);
        moveX(stepsize);
    }

    X.setRPM(STANDARD_SPEED);
    //move out on the other side, to a safe distance
    moveX(SAFEDISTANCE);

    //close the shutter
    closeShutter();
    delay(100);

    //rapid back to safe
    moveToX(-SAFEDISTANCE);

    // change line to next.
    changeLine();
}



void parseScanCommand(){
    // Response to SCAN command
    char *arg;
    arg = sCmd.next();
    int nn = 1;
    while ((arg != NULL) and (nn <= RESOLUTION)){
        scanLineContainer[nn] = atol(arg);
        Serial.print(atol(arg));
        arg = sCmd.next();
        nn += 1;
    }
    Serial.print("RETURN: Line finished, ");
    Serial.print(nn-1);
    Serial.println(" numbers received.");
    scanLine();
}

void parseMoveCommand() {
    // Command: MOVE X -30
    char *axis;
    char *displacement;

    axis = sCmd.next();
    displacement = sCmd.next();
    
    if (strcmp(axis, "X") ==0 ) {
        moveX(atol(displacement));
    }
    else if (strcmp(axis, "Y") ==0 ) {
        moveY(atol(displacement));
    }
    else {
        Serial.println("ERROR: move command not well formatted");
    }
}

void parseMoveToCommand() {
    // Command: MOVETO X -30
    char *axis;
    char *position;

    axis = sCmd.next();
    position = sCmd.next();
    
    if (strcmp(axis, "X") ==0 ) {
        moveToX(atol(position));
    }
    else if (strcmp(axis, "Y") ==0 ) {
        moveToY(atol(position));
    }
    else {
        Serial.println("ERROR: moveto command not well formatted");
    }
}

void returnPosition() {
    //Returns x, y position
    Serial.print("RETURN: position is: ");
    Serial.print(xpos);
    Serial.print(", ");
    Serial.println(ypos);

}

void homeAllAxis() {
    Xhome();
    Yhome();
    Serial.println("RETURN: All axis homed");
}

void unrecognized(const char *command) {
    Serial.println("ERROR: Command not recognized."); 
}

void setStepSize(){
    // Two cases: 
    // 1. If an argument is provided, a new stepsize is defined (and subsequently a different Area)
    // 2. (TBI) If no argument is provided, it will return the set stepsize.
    char *arg;
    arg = sCmd.next();    // Get the next argument from the SerialCommand object buffer
    if (arg != NULL) {    // As long as it existed, take it
      Serial.println("RETURN: Stepsize changed.");
      stepsize = atol(arg);
    }
    else {
      Serial.print("RETURN: Stepsize is: ");
      Serial.println(stepsize);
    }
}


void setCenter(){
    //This function resets the coordinate system to center, and ascertains remote-only control, and finally moves outside to the safe position.
    manualMode = false;
    scanMode = true;
    xpos = 127*stepsize;
    ypos = 127*stepsize;


    //move to safe corner
    moveToX(-SAFEDISTANCE);
    moveToY(-SAFEDISTANCE);
    safePosition = true;

}


void openShutter(){
    digitalWrite(SHUTTER_PIN, HIGH);
    Serial.println("RETURN: Shutter open.");
    shutterSafe = false;
}

void closeShutter(){
    digitalWrite(SHUTTER_PIN, LOW);
    Serial.println("RETURN: Shutter closed.");
    shutterSafe = true;
}


void moveX(long displacement){
    X.setRPM(STANDARD_SPEED);
    X.move(-displacement);
    xpos += displacement;
}

void moveY(long displacement){
    Y.setRPM(STANDARD_SPEED);
    Y.move(-displacement);
    ypos += displacement;
}

void moveToX(long position){
    long diff = xpos - position;
    if (abs(diff) > 3000){
        X.setRPM(FAST_SPEED);
        X.setSpeedProfile(X.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
        X.move(diff);
        xpos = position;
        X.setRPM(STANDARD_SPEED);
    }
    else
    {
        X.setSpeedProfile(X.CONSTANT_SPEED);
        X.move(diff);
        xpos = position;
    }
}

void moveToY(long position){
    long diff = ypos - position;
    //To handle speeds/errors in parsing, I might save myself some trouble and sanity, by clamping the admissible speeds.
    if (abs(diff) > 3000){
        Y.setRPM(FAST_SPEED);
        Y.setSpeedProfile(Y.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
        Y.move(diff);
        ypos = position;
        Y.setRPM(STANDARD_SPEED);
    }
    else
    {
        Y.setSpeedProfile(Y.CONSTANT_SPEED);
        Y.move(diff);
        ypos = position;
    }
}