//Arduino libraries
#include <ArduinoHardware.h>
#include <SoftwareSerial.h>
#include <avr/io.h>
#include <avr/interrupt.h>
//ROS Message
#include <ros.h>
#include <controller_input.h>
#include <controller_translational_constants.h>
#include <controller_rotational_constants.h>
#include <controller_onoff.h>
#include <thruster.h>
//Motor Drivers & Controller
#include <smcDriver.h>
#include <PID_v1.h> //based on http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/ 

/**********************************************************/
//INTERRUPT INITIALIZE
#define statusPin 11
int count;
int state;
//Initialize Timer0
void InitTimer0(void);
void StartTimer0(void);
void ledOn();
void ledOff();
void toggleLED();
ISR(TIMER0_COMPA_vect);
/***********************************************************/
//ROS INITIALIZE
int manual_speed[6];
long time_elapsed;

//initialize subscribers and publishers in ROS

ros::NodeHandle nh;

//teleopControl
void teleopControl(const bbauv_msgs::thruster &msg);
ros::Subscriber<bbauv_msgs::thruster> teleopcontrol_sub("teleop_controller",&teleopControl);

//Controller Mode
void updateControllerMode (const bbauv_msgs::controller_onoff &msg);
ros::Subscriber<bbauv_msgs::controller_onoff> controller_mode("controller_mode", &updateControllerMode);

//update PID Controller Constants; seperated into Rotation and Translational;
//Reason for separation: msg file cannot be too large, restriction from rosserial client
void updateTranslationalControllerConstants(const bbauv_msgs::controller_translational_constants &msg);
ros::Subscriber<bbauv_msgs::controller_translational_constants> pidconst_trans_sub("translational_constants", &updateTranslationalControllerConstants);
void updateRotationalControllerConstants(const bbauv_msgs::controller_rotational_constants &msg);
ros::Subscriber<bbauv_msgs::controller_rotational_constants> pidconst_rot_sub("rotational_constants", &updateRotationalControllerConstants);

// Update Controller Input; contains both the setpoitn and sensor feedback
void updateControllerInput(const bbauv_msgs::controller_input &msg);
ros::Subscriber<bbauv_msgs::controller_input> controller_sub("controller_input",&updateControllerInput);

//Publish thruster speed
bbauv_msgs::thruster thrusterSpeed;
ros::Publisher thruster_pub("thruster_feedback",&thrusterSpeed);

/*******ROS Subsriber Call Back Functions********/
void updateControllerMode (const bbauv_msgs::controller_onoff &msg);
void updateTranslationalControllerConstants(const bbauv_msgs::controller_translational_constants &msg);
void updateRotationalControllerConstants(const bbauv_msgs::controller_rotational_constants &msg);
void updateControllerInput(const bbauv_msgs::controller_input &msg);
void teleopControl(const bbauv_msgs::thruster &msg);

/***********************************************************/
//smcDriver INITIALIZE
#define rxPin 62 // Orange wire <-- receive from the 1st SMC Tx pin
#define txPin 37 // Red wire --> transmit to all SMCs Rx pin
smcDriver mDriver= smcDriver(rxPin,txPin);
/***********************************************************/
//PID INITIALIZE
bool inDepthPID, inHeadingPID, inForwardPID, inSidemovePID, inTopside, inSuperimpose, inTeleop;
double depth_setpoint,depth_input,depth_output;
double heading_setpoint,heading_input,heading_output;
double forward_setpoint,forward_input,forward_output;
double sidemove_setpoint,sidemove_input,sidemove_output;

PID depthPID(&depth_input, &depth_output, &depth_setpoint,1,0,0, DIRECT);
PID headingPID(&heading_input, &heading_output, &heading_setpoint,1,0,0, DIRECT);
PID forwardPID(&forward_input, &forward_output, &forward_setpoint,1,0,0, DIRECT);
PID sidemovePID(&sidemove_input, &sidemove_output, &sidemove_setpoint,1,0,0, DIRECT);

void runThruster();
void getTeleopControllerUpdate();
void getDepthPIDUpdate();
void getHeadingPIDUpdate();
void getForwardPIDUpdate();
void getSidemovePIDUpdate();
void setZeroHorizThrust();
void setZeroVertThrust();
void setHorizThrustSpeed();
void setVertThrustSpeed();
void calculateThrusterSpeed();
void superImposePIDoutput();
void rotatePIDoutput();

/******************************/
void init_interrupt();
void init_controller();
void init_pub_sub();

void setup()
{
  //initialize Motor driver and ROS
  init_interrupt();
  //mDriver.init();
  //nh.initNode();
  //init_pub_sub();
  //init_controller();
  //Note the sample time here is 50ms. ETS SONIA's control loop runs at 70ms.
}

  
void loop()
{  
/*
    if (millis() > time_elapsed)
    {
    superImposePIDoutput();
    //rotatePIDoutput();
    
    thruster_pub.publish(&thrusterSpeed);
    runThruster();
    time_elapsed = millis() + 50; //instead of delay(), use millis
    nh.spinOnce();
    }
    */
}


/************************************************/
//Interrupt implementation

void InitTimer0(void)
{
	//Set Initial Timer value
	TCNT0=0;
	//Place TOP timer value to Output compare register
	OCR0A=242;
	//Set CTC mode
	// and make toggle PD6/OC0A pin on compare match
	TCCR0A=0b10000010;
	// Enable interrupts.
	TIMSK0|=0b10;
}

void StartTimer0(void)
{
	//Set prescaler 64 and start timer
	TCCR0B=0b00000011;
	// Enable global interrupts
	sei();
}
void ledOn()
{
	digitalWrite(statusPin,HIGH);
}
void ledOff()
{
        digitalWrite(statusPin,LOW);
}

// Toggle the LED
void toggleLED()
{	
	if(state==1)
	{
		ledOn();
		state=0;
	}
	else
	{
		ledOff();
		state=1;
	}
}
// Set up the ISR for TOV0A
ISR(TIMER0_COMPA_vect)
{
  count++;
  if(count > 50){
    toggleLED();
    count=0;
  }
  //nh.spinOnce();
}
/************************************************/

void runThruster()
{
  
  mDriver.setMotorSpeed(1,thrusterSpeed.speed1);
  mDriver.setMotorSpeed(2,thrusterSpeed.speed2);
  mDriver.setMotorSpeed(3,thrusterSpeed.speed3);
  mDriver.setMotorSpeed(4,thrusterSpeed.speed4);
  mDriver.setMotorSpeed(5,thrusterSpeed.speed5);
  mDriver.setMotorSpeed(6,thrusterSpeed.speed6);

}

void getTeleopControllerUpdate()
{
  if(!inTeleop)
  {
  manual_speed[0]=0;
  manual_speed[1]=0;
  manual_speed[2]=0;
  manual_speed[3]=0;
  manual_speed[4]=0;
  manual_speed[5]=0;
  }
}

void getDepthPIDUpdate()
{
  depthPID.SetMode(inDepthPID);
  if(inDepthPID)
  {
    depthPID.Compute();
      /*
    if(inDepthPID && depth_setpoint == float(int(depth_input*10))/10) // && depth_setpoint-depth_input>-0.01)
    {
      
      thrusterSpeed.speed5=-1725;
      thrusterSpeed.speed6=-1725;
    }
    else
    {
      thrusterSpeed.speed5=depth_output+manual_speed[4];
      thrusterSpeed.speed6=depth_output+manual_speed[5];
    }*/
  }
  else
  {
    depth_output=0;
  }
}

void getHeadingPIDUpdate()
{
  
    if(heading_setpoint==0)
    {
      heading_setpoint=360;
    }
    if (heading_setpoint > (heading_input+180))
    {
      heading_input+=360;
    }  
  
  headingPID.SetMode(inHeadingPID);
  if(inHeadingPID)
  {
    headingPID.Compute();
  }
  else
  {
    heading_output=0;
  }
}

void getForwardPIDUpdate()
{
  forwardPID.SetMode(inForwardPID);
  if(inForwardPID)
  {
    forwardPID.Compute();
  }
  else
  {
    forward_output=0;
  }
}

void getSidemovePIDUpdate()
{
  sidemovePID.SetMode(inSidemovePID);
  if(inSidemovePID)
  {
    sidemovePID.Compute();
  }
  else
  {
    sidemove_output=0;
  }
  
}

void setZeroHorizThrust()
{

  thrusterSpeed.speed1=0;
  thrusterSpeed.speed2=0;
  thrusterSpeed.speed3=0;
  thrusterSpeed.speed4=0;
  
}



void setZeroVertThrust()
{
  thrusterSpeed.speed5=0;
  thrusterSpeed.speed6=0;

}


void setHorizThrustSpeed()
{
  thrusterSpeed.speed1=heading_output-forward_output+sidemove_output+manual_speed[0];
  thrusterSpeed.speed2=-heading_output-forward_output-sidemove_output+manual_speed[1];
  thrusterSpeed.speed3=heading_output+forward_output-sidemove_output+manual_speed[2];
  thrusterSpeed.speed4=-heading_output+forward_output+sidemove_output+manual_speed[3];   
}

void setVertThrustSpeed()
{
  thrusterSpeed.speed5=depth_output+manual_speed[4];
  thrusterSpeed.speed6=depth_output+manual_speed[5];
}

void calculateThrusterSpeed()
{
  getTeleopControllerUpdate();
  
  getDepthPIDUpdate();
  getHeadingPIDUpdate();
  getForwardPIDUpdate();
  getHeadingPIDUpdate();
  getSidemovePIDUpdate();
}

/* How are we sending same set of thrusters multiple PID outputs? */


void superImposePIDoutput()
{
  calculateThrusterSpeed();
  setHorizThrustSpeed();
  setVertThrustSpeed();
  //execute the calculated thruster speed

}

void rotatePIDoutput()
{
/*Experimental*/
  
  //Vertical
  inDepthPID=true;
  getDepthPIDUpdate();
  setVertThrustSpeed();
  runThruster();

  //Horizontal
  inHeadingPID=false; inForwardPID=true; inSidemovePID=false;
  getHeadingPIDUpdate();
  getForwardPIDUpdate();
  setHorizThrustSpeed();
  runThruster();
  //thruster_pub.publish(&thrusterSpeed);
  delay(5);

  inHeadingPID=true; inForwardPID=false; inSidemovePID=false;
  getHeadingPIDUpdate();
  getForwardPIDUpdate();
  setHorizThrustSpeed();
  runThruster();
  //thruster_pub.publish(&thrusterSpeed);
  delay(5);

/*
  nh.loginfo("Adjusting sideways");
  inHeadingPID=false; inForwardPID=false; inSidemovePID=true;
  getForwardPIDUpdate();
  setHorizThrustSpeed();
  runThruster();
  //setZeroHorizThrust();  
*/

}

/*********************************************************/

void updateControllerMode (const bbauv_msgs::controller_onoff &msg)
{
  inTopside=msg.topside;
  
  if(inTopside)
  {
  inTeleop=msg.teleop;    
  inDepthPID= msg.depth_PID;
  inForwardPID= msg.forward_PID;
  inSidemovePID= msg.sidemove_PID;
  inHeadingPID= msg.heading_PID;

  }
  else
  {
  inTeleop=false;
  inDepthPID=true;
  inForwardPID=true;
  inSidemovePID=true;
  inHeadingPID=true;
  }

}

void updateTranslationalControllerConstants(const bbauv_msgs::controller_translational_constants &msg)
{

  depthPID.SetTunings(msg.depth_kp,msg.depth_ki,msg.depth_kd);
  forwardPID.SetTunings(msg.forward_kp,msg.forward_ki,msg.forward_kd);
  sidemovePID.SetTunings(msg.sidemove_kp,msg.sidemove_ki,msg.sidemove_kd);

}

void updateRotationalControllerConstants(const bbauv_msgs::controller_rotational_constants &msg)
{
  headingPID.SetTunings(msg.heading_kp,msg.heading_ki,msg.heading_kd);
}


void updateControllerInput(const bbauv_msgs::controller_input &msg)
{
  /*setpoint and input of a PID must be of the same units */
  
  depth_input=msg.depth_input;
  depth_setpoint=msg.depth_setpoint;
  
  heading_input=msg.heading_input;
  heading_setpoint=msg.heading_setpoint;
  
  forward_input=msg.forward_input;
  forward_setpoint=msg.forward_setpoint;
  
  sidemove_input=msg.sidemove_input;
  sidemove_setpoint=msg.sidemove_setpoint;
 
}


void teleopControl(const bbauv_msgs::thruster &msg)
{
  manual_speed[0]=msg.speed1;
  manual_speed[1]=msg.speed2;
  manual_speed[2]=msg.speed3;
  manual_speed[3]=msg.speed4;
  manual_speed[4]=msg.speed5;
  manual_speed[5]=msg.speed6;
}


void init_controller()
{
  //initialize value for variables
  inTopside=true;
  inTeleop=false;
  
  inDepthPID= false;
  inHeadingPID= false;
  inForwardPID=false;
  inSidemovePID=false; 
  
                         
  for(int i=0;i<6;i++)
    manual_speed[i]=0;
  
  time_elapsed=0;
  depthPID.SetMode(AUTOMATIC);
  depthPID.SetSampleTime(5);
  depthPID.SetOutputLimits(-2560,2560);
  depthPID.SetControllerDirection(REVERSE);
  
  headingPID.SetMode(AUTOMATIC);
  headingPID.SetSampleTime(5);
  // too high a limit will result in too much overshoot.
  headingPID.SetOutputLimits(-800,800);
  headingPID.SetControllerDirection(DIRECT);
  
  forwardPID.SetMode(AUTOMATIC);
  forwardPID.SetSampleTime(5);
  forwardPID.SetOutputLimits(-1000,1260);
  forwardPID.SetControllerDirection(DIRECT);
  
  sidemovePID.SetMode(AUTOMATIC);
  sidemovePID.SetSampleTime(5);
  sidemovePID.SetOutputLimits(-500,500);
  sidemovePID.SetControllerDirection(DIRECT);
      
  //Initialize thruster ratio to 1:1:1:1:1:1
  float ratio[6]={0.8471, 0.9715, 0.9229, 0.9708, 0.8858, 1}; //see excel file in bbauv/clan folder
  mDriver.setThrusterRatio(ratio);
}


void init_pub_sub()
{
  nh.subscribe(controller_mode);
  nh.subscribe(controller_sub);
  nh.subscribe(pidconst_trans_sub);
  nh.subscribe(pidconst_rot_sub);
  nh.subscribe(teleopcontrol_sub);
  nh.advertise(thruster_pub);
}

void init_interrupt()
{
  pinMode(statusPin, OUTPUT); 
  count=0;
  state=0;
  InitTimer0();
  StartTimer0();
}
int main()
{
  setup();
  while(1)
  {
    loop();
  }
}
