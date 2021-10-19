#include <WiFi.h>
#include <HTTPSServer.hpp>
//provides a way to create certificats
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
//String to work with HTTPRequest/HTTPResponse
#include <String>
//file system
#include <SPIFFS.h>
//System for IODevices
#include "IOController.h"
#include "LED.h"
#include "Motor.h"

using namespace httpsserver;

//ssid and passwort for access point (doesn't work for some reason)
const char* ssid = "ESP32AP";
const char* password = "1234567890";

//initialise a sertificate and an https server
SSLCert* cert;
HTTPSServer* secureServer;

//initialise the IOController
uc2::IOController* ioContrTest;

void setup(){
  //test if file system works
  if(!SPIFFS.begin()){
    Serial.println("Error with SPIFFS");
    return;
  }

  //setup serial connection
  Serial.begin(115200);

  //create a certificate and test if it works
  Serial.println("Create Cert");
  cert = new SSLCert();
  int createCertResult = createSelfSignedCert(*cert, KEYSIZE_2048, "CN=espmicorscope, O=aaa_hrw, C=GER");
  if(createCertResult != 0){
    Serial.println("Error generating certificate");
    return;
  }

  //create a new HTTPSServer with the created certificate
  secureServer = new HTTPSServer(cert);

  //start the access Point and print the IP
  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

  //create a resource node, that waits for a HTTP GET for the index page and returns the index page
  ResourceNode* nodeIndex = new ResourceNode("/", "GET", [](HTTPRequest* req, HTTPResponse* res){
    File indexFile = SPIFFS.open("/index.html", "r");
    if(indexFile){
      res->print(indexFile.readString());
      indexFile.close();
    }
  });

  //resource node, that handels the style.css
  ResourceNode* nodeCss = new ResourceNode("/style.css", "GET", [](HTTPRequest* req, HTTPResponse* res){
    File styleFile = SPIFFS.open("/style.css", "r");
    if(styleFile){
      res->print(styleFile.readString());
      styleFile.close();
    }
  });

  //resource node, that handels the index.js
  ResourceNode* nodeJS = new ResourceNode("/index.js", "GET", [](HTTPRequest* req, HTTPResponse* res){
    File jsFile = SPIFFS.open("/index.js", "r");
    if(jsFile){
      res->print(jsFile.readString());
      jsFile.close();
    }
  });

  //resource node, for a POST request, to add an LED
  ResourceNode* nodeAddLED = new ResourceNode("/post/add/LED", "POST", [](HTTPRequest* req, HTTPResponse* res){
    size_t bodyLength = req->getContentLength();
    char* buff = new char[bodyLength+1];
    buff[bodyLength] = '\0';
    req->readChars(buff, bodyLength);
    int pin;
    pin = atoi(buff);
    ioContrTest->addODevice(new uc2::LED(pin));
    //TODO!!! change it so that only the added device initialisation is run
    ioContrTest->controllerInit();
    delete buff;
  });

  ResourceNode* nodeChangeLEDState = new ResourceNode("/post/change/LED", "POST", [](HTTPRequest* req, HTTPResponse* res){
    size_t bodyLength = req->getContentLength();
    char* buff = new char[bodyLength+1];
    buff[bodyLength] = '\0';
    req->readChars(buff, bodyLength);
    int state;
    state = atoi(buff);
    ((uc2::LED*)ioContrTest->oDevices.back())->turnOn(state);
  });

  //initialise node for server, start the server and test if its running
  secureServer->registerNode(nodeIndex);
  secureServer->registerNode(nodeCss);
  secureServer->registerNode(nodeJS);
  secureServer->registerNode(nodeAddLED);
  secureServer->registerNode(nodeChangeLEDState);
  secureServer->start();
  if(secureServer->isRunning()){
    Serial.println("Ready");
  }

  //initialises the IOController
  ioContrTest = new uc2::IOController();
  //adds a Motor to the device list
  ioContrTest->addODevice(new uc2::Motor(2,15,13,12));
  //runs the initialise methods of LED
  ioContrTest->controllerInit();
}

void loop(){
  //run the serverloop every 10 ms
  secureServer->loop();
  //updates the state of all devices every loop
  ioContrTest->controllerUpdate();
  delay(10);
}