
#include <SPI.h>         
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Time.h>
#include <LiquidCrystal.h>
#include <stdlib.h>
#include <dht.h>

unsigned long sendNTPpacket(IPAddress& address);
//#define dhttype DHT22;
dht DHT;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

//standard port to start arduino UDP on
unsigned int localPort = 8888;      
//string for parsing GET request
String  readString = String(30);    
//activity queue
String todo="";


const int port = 80;
unsigned long time_off = 0;
unsigned long time_on = 0;
const int tolerance = 2;
int current_temp = 74;
int set_temp = 80;
unsigned long change_time=0;
unsigned long last_update=0;
unsigned long time_on_today;
unsigned long time_off_today;
unsigned long time_on_yesterday;
unsigned long time_off_yesterday;

bool cool_on = false;
bool cool_mode = true;
bool heat_on = false;
bool heat_mode = false;
bool system_on = true;
bool compressor_timingout = false;
bool lcdsetup = false;

bool accent_on = false;
bool ceiling_on = false;
bool fan_on = false;
bool dining_on = false;
bool kitchen_on = false;

int heat_relay=29;
int cool_relay=28;
int acfan_relay=27;
int ceiling_relay=26;
int fan_relay=25;
int dining_relay=24;
int accent_relay=23;
int kitchen_relay=22;

LiquidCrystal lcd(30,31,32,33,34,35,36);

int toggle_butt = 39;
int up_butt = 38;
int down_butt = 37;

IPAddress ip(10,2,1,20);
IPAddress logger(10,2,1,8);
boolean logging = false;
int loggerPort = 514;
IPAddress timeServer(129,6,15,28); // time-a.nist.gov NTP server
IPAddress gateway(10,2,1,1);

const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets 

int humidity;

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;
EthernetServer server(port);
void setup() 
{
  lcd.begin(20,4);
  lcd.print("MitchMania Labs");
  lcd.setCursor(0,1);
  lcd.print("Thermostat V1.0");
  delay(2000);
  pinMode(up_butt,INPUT);
  pinMode(down_butt,INPUT);
  pinMode(toggle_butt,INPUT);
  pinMode(cool_relay,OUTPUT);
  pinMode(heat_relay,OUTPUT);
  pinMode(acfan_relay,OUTPUT);
  pinMode(accent_relay,OUTPUT);
  pinMode(ceiling_relay,OUTPUT);
  pinMode(fan_relay,OUTPUT);
  pinMode(kitchen_relay,OUTPUT);
  pinMode(dining_relay,OUTPUT);
  digitalWrite(up_butt,HIGH);
  digitalWrite(down_butt,HIGH);
  digitalWrite(toggle_butt,HIGH);
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // start Ethernet and UDP
  Ethernet.begin(mac,ip,gateway,gateway);
  //standard port to start arduino UDP on
  Udp.begin(localPort);

  DHT.read22(40);
  
  int dhttemp = DHT.temperature;
  humidity = DHT.humidity;
  delay(10000);
  current_temp=dhttemp*9/5+32;
}

void loop()
{
  if (now()-last_update>10){
    DHT.read22(40);
    current_temp=DHT.temperature*9/5+32;
    last_update=now();
    humidity = DHT.humidity;
  }

  //0=time not set, 2=time set
  if (timeStatus() == 0){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Getting time");
    gettime();
  }
  //Start the thermostat
  delay(100);
  //Fill LCD
  if(lcdsetup==false){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Got Time");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(String(set_temp)+" Set");
    lcd.setCursor(0,1);
    lcd.print(String(current_temp)+" Current");
    lcdsetup=true;
    lcd.setCursor(0,2);
    if(cool_mode==true){
      lcd.print("COOL");
    }else if(heat_mode==true){
      lcd.print("     HEAT");
    }else if(system_on==false){
      lcd.print("          OFF");
    }
  }
  //Print the time on LCD
  lcd.setCursor(10,0);
  if(hour()<10){
    lcd.print("0"+String(hour())+":");
  }else{
    lcd.print(String(hour())+":");
  }
  if(minute()<10){
    lcd.print("0"+String(minute())+":");
  }else{
    lcd.print(String(minute())+":");
  }
  if(second()<10){
    lcd.print("0"+String(second()));
  }else{
    lcd.print(String(second()));
  }
  //Set change time to now to start the 10 second countdown between a temp change and an event
  if(change_time==0){change_time=now();}
  if ((cool_on==true && cool_mode==true) || (cool_on==true && system_on==false)){
    //cool is on check to turn off or cool is on and system is off
    if (current_temp-set_temp<=0 && cool_on==true){
      //turn off
      cool_on=false;
      change_time=now();
      todo="cooloff";
      time_off=now();
    }else if (system_on==false && cool_on==true){
      cool_on=false;
      change_time=now();
      todo="cooloff";
      time_off=now();
    }
  }
  if (cool_on==false && cool_mode==true){
    if (current_temp-set_temp>=tolerance && cool_on==false){
      //turn on
      cool_on=true;
      change_time=now();
      todo="coolon";
    }
  }
  if (heat_on==true && heat_mode==true){
    if (current_temp-set_temp>=0 && heat_on==true){
      //turn heat off
      heat_on=false;
      change_time=now();
      todo="heatoff";
    }else if (system_on==false&&heat_on==true){
      heat_on=false;
      change_time=now();
      todo="heatoff";
    }
  }
  if (heat_on==false && heat_mode==true){
    if (current_temp-set_temp<= (-1 * tolerance)&&heat_on==false){
      //turn heat on
      heat_on=true;
      change_time=now();
      todo="heaton";
    }
  }
  doo(todo,change_time);
  compressor_timingout=false;
  dailytotal();
  
  //start webserver
  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (readString.length() < 30) { //read char by char HTTP request, max length is 30 chars
          readString.concat(c); } //store characters to string
        if (c == '\n') { //if HTTP request has ended
          int Le = readString.indexOf("t="); //temp change request
          if (Le > 1){
            char read_temp[3]; 
            String temp_str = readString.substring(Le+2,(Le+4));

            temp_str.toCharArray(read_temp,3);
            int num = atoi(read_temp);

            set_temp=num;
          }
          int Te = readString.indexOf("toggle=");
          if (Te > 1){
            //TODO if heat or cool on, turn off, BUG
            //toggle heat cool off
            if (heat_mode==true){
              heat_mode=false;
              cool_mode=true;
              system_on=true;
            }
            else if(cool_mode==true){
              heat_mode=false;
              cool_mode=false;
              system_on=false;
            }
            else if (system_on==false){
              heat_mode=true;
              cool_mode=false;
              system_on=true;
            }
          }
          //toggle accent light
          if (readString.indexOf("accent=")>1){
            if (accent_on==true){
              digitalWrite(accent_relay,LOW);
              accent_on=false;
            }else{
              digitalWrite(accent_relay,HIGH);
              accent_on=true;
            }
          }
          //toggle
          if (readString.indexOf("ceiling=")>1){
            if (ceiling_on==true){
              digitalWrite(ceiling_relay,LOW);
              ceiling_on=false;
            }else{
              digitalWrite(ceiling_relay,HIGH);
              ceiling_on=true;
            }
          }
          //toggle          
          if (readString.indexOf("fan=")>1){
            if (fan_on==true){
              digitalWrite(fan_relay,LOW);
              fan_on=false;
            }else{
              digitalWrite(fan_relay,HIGH);
              fan_on=true;
            }
          }
          if (readString.indexOf("dining=")>1){
            if (dining_on==true){
              digitalWrite(dining_relay,LOW);
              dining_on=false;
            }else{
              digitalWrite(dining_relay,HIGH);
              dining_on=true;
            }
          }
          //toggle   
          if (readString.indexOf("kitchen=")>1){
            if (kitchen_on==true){
              digitalWrite(kitchen_relay,LOW);
              kitchen_on=false;
            }else{
              digitalWrite(kitchen_relay,HIGH);
              kitchen_on=true;
            }
          }         
          client.println("HTTP/1.1 200 OK"); //begin http response
          //JSON response
          if (readString.indexOf("/json")>1){
            client.println("Content-Type: application/json");
            client.println();
            client.println("{");
            client.println("  \"response\": [");
            client.println("    {");
            client.println("      \"description\": \"Thermostat Variable Dump\",");
            client.println("      \"current_temp\": " + String(current_temp)+",");
            client.println("      \"humidity\": " + String(humidity) + ",");
            client.println("      \"set_temp\": " + String(set_temp)+",");
            client.println("      \"tolerance\": " + String(tolerance)+",");
            client.println("      \"cool_on\": " + String(cool_on)+",");
            client.println("      \"heat_on\": " + String(heat_on)+",");
            client.println("      \"time_on\": " + String(time_on)+",");
            client.println("      \"time_off\": " + String(time_off)+",");
            client.println("      \"time_on_today\": " + String(time_on_today)+",");
            client.println("      \"time_off_today\": " + String(time_off_today)+",");
            client.println("      \"current_time\": " + String(now())+",");
            client.println("      \"cool_mode\": " + String(cool_mode)+",");
            client.println("      \"heat_mode\": " + String(heat_mode)+",");
            client.println("      \"system_on\": " + String(system_on)+",");
            client.println("      \"accent_on\": " + String(accent_on)+",");
            client.println("      \"ceiling_on\": " + String(ceiling_on)+",");
            client.println("      \"fan_on\": " + String(fan_on)+",");
            client.println("      \"dining_on\": " + String(dining_on)+",");
            client.println("      \"kitchen_on\": " + String(kitchen_on)+",");
            client.println("      \"Device\": \"Thermostat\"");
            client.println("    }");
            client.println("  ]");
            client.println("}");
          }  
          else{
            //BEGIN HTML RESPONSE
            client.println("Content-Type: text/html");
            client.println();
            //the following line is used for auto refresh
            //client.println("<html><head><meta http-equiv='refresh' content='15'></head>");
            client.print  ("<html><body style=background-color:white>"); //set background to white
            client.println("<font color='red'><h1>THERMOSTAT</font></h1>");//send first heading
  
            client.println("Set Temperature = <big><big><big><big>" + String(set_temp) + "</big></big></big></big><form method='get'"+
              " action='/'><textarea name='t' cols='3' rows='1' style='resize:none'>"
              + String(set_temp) + "</textarea>");

            client.println("<input type='submit' value='Submit' />");
            client.println("<br/>Current temp = <big><big><big><big>" + String(current_temp) + "</big></big></big></big><br/>Tolerance = " + String(tolerance));
            //cool_on
            client.println("<br/>Cool is ");
            if (cool_on==true){
              client.println("on");
            }else if (cool_on==false){
              client.println("off");
            }
            client.println("<br />time_on = "+String(time_on)+ "<br />time_off = " + String(time_off) + "<br/>time_on_today = " 
              + String(time_on_today) + "<br />time_off_today = " + String(time_off_today) + "<br />");
  
            client.println("humidity = ");
            client.println(humidity); 
  
            client.println("<br />current time " + String(now())); // + ;
            
            //cool_mode
            if(compressor_timingout==true){
              client.println("<br/>Compressor is timing out");
            }
            client.println("<br/>Cooling is ");
            if (cool_mode==true){
              client.println("enabled");
            }else if (cool_mode==false){
              client.println("disabled");
            }
            //heat_on
            client.println("<br/>Heat is ");
            if (heat_on==true){
              client.println("on");
            }else if (heat_on==false){
              client.println("off");
            }
            //heat_mode
            client.println("<br/>Heating is ");
            if (heat_mode==true){
              client.println("enabled");
            }else if (heat_mode==false){
              client.println("disabled");
            }
            client.println("<br/>system_on="+String(system_on));
            client.println("<br/>");
            client.println("<button name='toggle'>Heat/Cool/Off</button>");
            
            client.println("<br/><button name='accent'>Accent Light</button>");
            if(accent_on==true){
              client.println("On");
            }else{
              client.println("Off");
            }
            client.println("<br/><button name='ceiling'>Hallway Light</button>");  
            if(ceiling_on==true){
              client.println("On");
            }else{
              client.println("Off");
            }
            client.println("<br/><button name='fan'>Outside Light</button>");
            if(fan_on==true){
              client.println("On");
            }else{
              client.println("Off");
            }
            client.println("<br/><button name='dining'>Dining Room Light</button>");
            if(dining_on==true){
              client.println("On");
            }else{
              client.println("Off");
            }
            client.println("<br/><button name='kitchen'>Kitchen Light</button>");
            if(kitchen_on==true){
              client.println("On");
            }else{
              client.println("Off");
            }
            client.println("<br/></form><hr /></body></html>");
          }
          //End HTML response
          readString=""; //clearing request string for next read
          client.stop(); //stopping client
        }
      }
    }
  }
  //end http, start lcd
  lcd.setCursor(0,0);
  lcd.print(set_temp);
  lcd.setCursor(0,1);
  lcd.print(current_temp);
  lcd.setCursor(0,2);
  if(cool_on==true){
    lcd.print("COOL ON");
  }else if(heat_on==true){
    lcd.print("        HEAT ON");
  }else lcd.print("                ");
  lcd.setCursor(0,3);
  if(cool_mode==true){
    lcd.print("COOL         ");
    delay(400);
  }else if(heat_mode==true){
    lcd.print("     HEAT    ");
    delay(400);
  }else if(system_on==false){
    lcd.print("          OFF");
    delay(400);
  }
  //end lcd, start reading buttons
  if(digitalRead(toggle_butt)==LOW){
    if (heat_mode==true){
      heat_mode=false;
      cool_mode=true;
      system_on=true;
      //delay(1000); //created delay between button press and lcd write
    }
    else if(cool_mode==true){
      heat_mode=false;
      cool_mode=false;
      system_on=false;
      //delay(1000);
    }
    else if (system_on==false){
      heat_mode=true;
      cool_mode=false;
      system_on=true;
      //delay(1000);
    }
  }
  if(digitalRead(up_butt)==LOW){
    set_temp=set_temp+1;
  }
  if(digitalRead(down_butt)==LOW){
    set_temp=set_temp-1;
  }
}

void doo(String todo1, unsigned long change_time){
  if (todo1=="coolon"&&now()-change_time>10){
    //coolon
    time_on=now();
    time_off_today+=time_on-time_off;
    if (now()-time_off>350){
      digitalWrite(cool_relay,HIGH);
      digitalWrite(acfan_relay,HIGH);
      //logging to centralized server, TODO refactor
      if (logging=true){
        Udp.beginPacket(logger, loggerPort);
        Udp.write("Cool on, set temp: ");
        String strBuf = String(set_temp);
        char charBuf[3];
        strBuf.toCharArray(charBuf,3);
        Udp.write(charBuf);
        Udp.write(", current temp: ");
        strBuf = String(current_temp);
        strBuf.toCharArray(charBuf,3);
        Udp.write(charBuf);
        Udp.endPacket();
      }
      //end logging
      todo1="";
      todo="";
      lcd.setCursor(8,2);
      lcd.print("     ");
    }else if (digitalRead(cool_relay)==false){ //if added to prevent infinite cool on wait message because
    // time off was never set because compressor was never turned off
      lcd.setCursor(8,2);
      lcd.print("WAIT! ");
      lcd.print(350-now()+time_off);
    }
  }
  if (todo1=="cooloff"&&now()-change_time>10){
    //cooloff
    todo1="";
    todo="";
    digitalWrite(cool_relay,LOW);
    digitalWrite(acfan_relay,LOW);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (logging=true){    
      Udp.write("Cool off, set temp: ");
      String strBuf = String(set_temp);
      char charBuf[3];
      strBuf.toCharArray(charBuf,3);
      Udp.write(charBuf);
      Udp.write(", current temp: ");
      strBuf = String(current_temp);
      strBuf.toCharArray(charBuf,3);
      Udp.write(charBuf);
      Udp.endPacket();
    }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    time_off=now();
    time_on_today+=time_off-time_on;
  }
  if (todo1=="heaton"&&now()-change_time>10){
    //heaton
    if (now()-time_off>350){
      digitalWrite(heat_relay,HIGH);
      digitalWrite(acfan_relay,HIGH);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      if (logging=true){
        Udp.beginPacket(logger, loggerPort);
        Udp.write("Heat on, set temp: ");
        String strBuf = String(set_temp);
        char charBuf[3];
        strBuf.toCharArray(charBuf,3);
        Udp.write(charBuf);
        Udp.write(", current temp: ");
        strBuf = String(current_temp);
        strBuf.toCharArray(charBuf,3);
        Udp.write(charBuf);
        Udp.endPacket();
      }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      todo1="";
      todo="";
      lcd.setCursor(8,2);
      lcd.print("     ");    
    }else if (digitalRead(heat_relay)==false){
      lcd.setCursor(8,2);
      lcd.print("WAIT!");
    }
  }
  if (todo1=="heatoff"&&now()-change_time>10){
    //heatoff
    todo1="";
    todo="";
    digitalWrite(heat_relay,LOW);
    digitalWrite(acfan_relay,LOW);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (logging=true){
      Udp.beginPacket(logger, loggerPort);
      Udp.write("Cool on, set temp: ");
      String strBuf = String(set_temp);
      char charBuf[3];
      strBuf.toCharArray(charBuf,3);
      Udp.write(charBuf);
      Udp.write(", current temp: ");
      strBuf = String(current_temp);
      strBuf.toCharArray(charBuf,3);
      Udp.write(charBuf);
      Udp.endPacket();
    }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    time_off=now();
  }
}

void gettime(){
  sendNTPpacket(timeServer);
  //wait for reply
  delay(1000);
  if ( Udp.parsePacket() ) {  
    //packet recieved
    Udp.read(packetBuffer,NTP_PACKET_SIZE);
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    unsigned long secsSince1900 = highWord << 16 | lowWord;  
    const unsigned long seventyYears = 2208988800UL;     
    unsigned long epoch = secsSince1900 - seventyYears;
    setTime(epoch-14400);
  }
  //if failed to recieve response, delay before retry
  delay(10000); 
}

unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  //form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  //send request	   
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}

void dailytotal(){
  if (now()/86400==0){
    time_on_yesterday = time_on_today;
    time_off_yesterday = time_off_today;
  }
}
