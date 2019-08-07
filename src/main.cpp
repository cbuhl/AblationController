#include <Arduino.h>
#include "A4988.h"
#include <SerialCommand.h>
 /*
 Note that in the SerialCommand.h you need to edit the file, to allow for the large number of parameters:

 // Size of the input buffer in bytes (maximum length of one command plus arguments) (was 32)
#define SERIALCOMMAND_BUFFER 1100
// Maximum length of a command excluding the terminating null (was 8)
#define SERIALCOMMAND_MAXCOMMANDLENGTH 16
  */
#include <Ticker.h>
#include <Wire.h> 
#include <LiquidCrystal.h>



void display_update();
void check_for_estop();
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
void executeScanLine();
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
#define SHUTTER_PIN 13
#define RESOLUTION 256
#define SAFEDISTANCE 500 //safe distance in steps, from the edge of the relevant field (0,0).


//RS, ENABLE, D1, D2, D3, D4
LiquidCrystal lcd(46, 47, 50, 51, 52, 53);
A4988 X(MOTOR_STEPS, XDIR, XSTEP);
A4988 Y(MOTOR_STEPS, YDIR, YSTEP);
SerialCommand sCmd;     // The SerialCommand object for parsing arguments from Serial.
//Ticker SerialPrintInfo(print_update,  1000);
Ticker DisplayPrint(display_update, 100);
Ticker EmergencyStop(check_for_estop, 100);
//Ticker CheckJoystick(joystickMove, 1, 0, 2);

// Define relevant states:
bool xHomed = false;
bool yHomed = false;
bool manualMode = true;
bool scanMode = false;
bool safePosition = false;
bool shutterSafe = false;
// received states
bool aReceived = false;
bool bReceived = false;
bool cReceived = false;
bool dReceived = false;

//define storage array for scan lines
int scanLineContainer[RESOLUTION];
long stepsizeX = 31; //The stepsize per pixel
long stepsizeY = 25; //The stepsize per pixel
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
    sCmd.addCommand("CENTER",   setCenter);
    sCmd.addCommand("OPEN",     openShutter);
    sCmd.addCommand("CLOSE",    closeShutter);
    sCmd.addCommand("EXEC",     executeScanLine);
    sCmd.setDefaultHandler(unrecognized);


    lcd.begin(24,2);
    lcd.clear();
    DisplayPrint.start();
    EmergencyStop.start();
    //CheckJoystick.start();
    pinMode(XEND, INPUT_PULLUP);
    pinMode(YEND, INPUT_PULLUP);
    pinMode(SHUTTER_PIN, OUTPUT);


    // Set target motor RPM to 1RPM and microstepping to 16 (full step mode)
    Serial.begin(57600);
    Serial.println("Ablation controller, V1");
    lcd.setCursor(4,0);
    lcd.print("github.com/cbuhl/");
    lcd.setCursor(3, 1);
    lcd.print("AblationController");


    X.begin(STANDARD_SPEED, 16);
    Y.begin(STANDARD_SPEED, 16);
    X.setSpeedProfile(X.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
    Y.setSpeedProfile(Y.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);

    closeShutter();

    X.enable();
    Y.enable();

    delay(2000);
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
    EmergencyStop.update();
    
    
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void check_for_estop() {
    if (!manualMode) {
        if ((analogRead(XJOYSTICK)>620) and (analogRead(YJOYSTICK)<400)){
            closeShutter();
            X.stop();
            Y.stop();
            // I considered if it should regain control to the user, but decided against it. 
            // You start out in manual mode, and 
        }
    }
}

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
            xpos += sign*X.getRPM();
        }
        
        if (!yDead) {
            int newspeed = yJoyPos-511;
            int sign = newspeed/abs(newspeed);

            Y.setRPM(scaleJoystickPosition(newspeed));
            //Serial.println(Y.getRPM());
            Y.setSpeedProfile(Y.CONSTANT_SPEED);
            Y.move(sign * Y.getRPM());
            ypos += sign*Y.getRPM();
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
    // command MAN
    manualMode = true;
    scanMode = false;
    Serial.println("RETURN: Joystick enabled");
}

void disableJoystick() {
    // command REM
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
    moveY(stepsizeY);

    

}

void scanLine(){
    //move to safe position, first line, 0 (backlash correction), for the first line scan.
    if (safePosition) {
        moveToY(0);
        safePosition = false;
    }
    //scan the line
    X.setSpeedProfile(X.LINEAR_SPEED, STANDARD_ACCEL, STANDARD_ACCEL);
    moveToX(0);
    openShutter();
    X.setRPM(350);
    //delay before start
    delay(100);
    for (int i = 0; i < RESOLUTION; i++)
    {
        int waitingTime = scanLineContainer[i];
        //sanity check on the scan speed
        if (waitingTime <= 1){
          waitingTime = 0;
        }
        if (waitingTime >= 500){
          waitingTime = 500;
        }
        //X.setRPM(rpm);
        delay(waitingTime);
        moveX(stepsizeX);
    }

    delay(100);

    Serial.println("RETURN: Line scanned succesfully");
    delay(100);

    X.setRPM(STANDARD_SPEED);
    //move out on the other side, to a safe distance
    moveX(SAFEDISTANCE);

    //close the shutter
    closeShutter();
    delay(100);

    //rapid back to safe
    moveToX(-SAFEDISTANCE);

    // change line to next, and inhibit start of next line.
    changeLine();
    aReceived = false;
    bReceived = false;
    cReceived = false;
    dReceived = false;
}



void parseScanCommand(){
    // Response to SCAN command
    // command SCAN {byte string of integer numbers describing the wait time at each pixel}
    // command SCAN 345 44 234 5 234 33 23 12
    char *arg;
    char *section;
    //int scanLineContainer[RESOLUTION];
    section = sCmd.next();

    int total = 0;

    // decide between the various sections of the array:
    if (strcmp(section, "A") ==0 ) {
        int nn = 0;
        arg = sCmd.next();
        while ((arg != NULL) and (nn <= RESOLUTION/4)){
            scanLineContainer[nn] = atol(arg);
            arg = sCmd.next();
            nn += 1;
            total += 1;
        }

        aReceived = true;
    }

    else if (strcmp(section, "B") ==0 ) {
        int nn = 64;
        arg = sCmd.next();
        while ((arg != NULL) and (nn <= RESOLUTION/2)){
            scanLineContainer[nn] = atol(arg);
            arg = sCmd.next();
            nn += 1;
            total += 1;
        }

        bReceived = true;
    }

    else if (strcmp(section, "C") ==0 ) {
        int nn = 128;
        arg = sCmd.next();
        while ((arg != NULL) and (nn <= RESOLUTION*3/4)){
            scanLineContainer[nn] = atol(arg);
            arg = sCmd.next();
            nn += 1;
            total += 1;
        }

        cReceived = true;
    }

    else if (strcmp(section, "D") ==0 ) {
        int nn = 192;
        arg = sCmd.next();
        while ((arg != NULL) and (nn <= RESOLUTION)){
            scanLineContainer[nn] = atol(arg);
            arg = sCmd.next();
            nn += 1;
            total += 1;
        }

        dReceived = true;
    }

    else {
        Serial.println("ERROR: wrong segment indicator in scan line transmission.");
        Serial.println(section);
        Serial.println(arg);
    }

    Serial.print("RETURN: Line finished, ");
    Serial.print(total);
    Serial.println(" numbers received.");
}

void executeScanLine() {
    if (aReceived and bReceived and cReceived and dReceived) {
        
        scanLine();
    }
    else {
        Serial.println("ERROR: Not all scan segments have been received");
    }
}

void parseMoveCommand() {
    // Relative movement
    // Command: MOVE X -30
    // command MOVE Y 3332
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
    // command MOVETO Y 3433
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
    // command GETPOS
    Serial.print("RETURN: position is: ");
    Serial.print(xpos);
    Serial.print(", ");
    Serial.println(ypos);

}

void homeAllAxis() {
    // command HOME
    Xhome();
    Yhome();
    Serial.println("RETURN: All axis homed");
}

void unrecognized(const char *command) {
    Serial.println("ERROR: Command not recognized."); 
}

void setStepSize() {
    // Two cases: 
    // 1. If an axis argument is provided, a new stepsize is defined (and subsequently a different Area)
    // 2. (TBI) If no argument is provided, it will return the set stepsize.
    // command STEP
    // command STEP X 34
    // command STEP Y 33
    char *axis;
    char *tempStepsize;

    axis = sCmd.next();
    tempStepsize = sCmd.next();

    if ((axis != NULL) or (tempStepsize != NULL)){
        
        if (strcmp(axis, "X") ==0 ) {
            Serial.println("RETURN: Stepsize changed on X.");
            stepsizeX = atol(tempStepsize);
        }
        else if (strcmp(axis, "Y") ==0 ) {
            Serial.println("RETURN: Stepsize changed on Y.");
            stepsizeY = atol(tempStepsize);
        }
        else {
            Serial.println("ERROR: stepsize axis specifier not understood");
        }
    }
    else {
        Serial.print("RETURN: Stepsize is (x,y): ");
        Serial.print(stepsizeX);
        Serial.print(", ");
        Serial.println(stepsizeY);
    }
}


void setCenter(){
    //This function resets the coordinate system to center, and ascertains remote-only control, and finally moves outside to the safe position.
    // command CENTER
    manualMode = false;
    scanMode = true;
    xpos = RESOLUTION/2*stepsizeX;
    ypos = RESOLUTION/2*stepsizeY;


    //move to safe corner
    moveToX(-SAFEDISTANCE);
    moveToY(-SAFEDISTANCE);
    safePosition = true;

}


void openShutter(){
    // command OPEN
    digitalWrite(SHUTTER_PIN, HIGH);
    Serial.println("RETURN: Shutter open.");
    shutterSafe = false;
}

void closeShutter(){
    // command CLOSE
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