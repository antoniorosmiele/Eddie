/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <vector>
#include <string>
#include <json/json.h>
#include <fstream>
#include "VirtualizationReceiver.h"
#include "argparse.hpp"
#include "EddieVirtualAlarm.h"
#include "FakeResource.h"
#include "EddieActuatorsWater.h"
#include "EddieAllarmSilent.h"
#include "EddieAudioSpeaker.h"
#include "EddieBulb.h"
#include "EddieCalculatorCPU.h"
#include "EddieCamera.h"
#include "EddieDataset.h"
#include "EddieDoor.h"
#include "EddieFacialRecognizer.h"
#include "EddieMattressCover.h"
#include "EddieMicrophone.h"
#include "EddieMonitorVideo.h"
#include "EddieSensorBracelet.h"
#include "EddieTemperatureSensor.h"
#include "EddieService.h"
#include "EddieTent.h"
#include "EddieVibrationAllarm.h"
#include "EddieAllarmNoisy.h"
#include "EddieBlinkingLamp.h"

using namespace std;

int main(int argc, char *argv[]) {
    /*
    Note sulle inizializzazioni delle risorse
    Ogni risorsa nella stringa degli attributi deve avere l'ID!
    Esse sono le chiavi delle risorse senza di esse la selezione non è possibile
    La stringa degli attributi messa nel costruttore è quella che viene poi restituita sia ai
    client che fanno la richiesta sia quando si chiama il metodo get_resources_in_linkformat() dentro
    il framework (Esso restituisce tutte le risorse registrate in un agent in formato Link con i relativi attributi)
    Si tenga nota che quindi in questi due casi non vengono restituiti tutti gli attributi presenti nel JSON
    Se si vuole tutto il JSON di una risorsa bisogna richiederlo manualmente
    */
    argparse::ArgumentParser program("eddie-virt-server");

    program.add_argument("--ip", "-a")
            .help("Ip address")
            .default_value(std::string{"auto"});

    program.add_argument("--port", "-p")
            .help("Port number")
            .scan<'i', int>()
            .default_value(5683);

    program.add_argument("--exampleres", "-e")
            .help("Publish example lamp resources")
            .default_value(false)
            .implicit_value(true);

    program.add_argument("--one", "-1")
            .help("First node server")
            .default_value(false)
            .implicit_value(true);

    program.add_argument("--two", "-2")
            .help("Second node server")
            .default_value(false)
            .implicit_value(true);      

    program.add_argument("--three", "-3")
            .help("Third node server")
            .default_value(false)
            .implicit_value(true);  

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    auto port_number = program.get<int>("port");
    auto ip = program.get<std::string>("ip");
    if (ip == "auto") ip = "";

    VirtualizationReceiver receiver = VirtualizationReceiver(ip, std::to_string(port_number));
    LOG_DBG("Receiver creation complete");

    std::vector<std::string> allName1 = {"ActuatorWater1","AllarmNoisy2","AllarmSilent1","AudioSpeaker1","AudioSpeaker3","AudioSpeaker4",
                                        "AudioSpeaker5","AudioSpeaker6","Bulb1","CalculatorCPU1","Camera7","Camera8","Camera9","dataset1","dataset2",
                                        "Door2","Door4","Door6","FacialRecognizer1","FacialRecognizer3","MattressCover3","Microphone7","MonitorVideo1"
                                        ,"MonitorVideo3","MonitorVideo4","MonitorVideo5","MonitorVideo6","SensorBracelet1",
                                        "Service1","TemperatureSensor4","Tent2","VibrationAllarm1"};

    std::vector<std::string> allName2 = {"ActuatorWater2","AllarmNoisy1","AllarmSilent2","AudioSpeaker2","Bulb2","CalculatorCPU2",
                                        "Camera2","Camera10","Camera11","Door3","Door5","Door7","FacialRecognizer2","FacialRecognizer4","MattressCover1",
                                        "Microphone2","MonitorVideo2","SensorBracelet2","Service2","TemperatureSensor1","TemperatureSensor2","TemperatureSensor3","Tent3"
                                        ,"VibrationAllarm2"};
    
    std::vector<std::string> allName3 = {"ActuatorWater3","AllarmNoisy3","AllarmSilent3","AudioSpeaker7","Bulb3","Bulb4",
                                        "CalculatorCPU3","Camera1","Camera3","Camera4","Camera5","Camera6","Camera12","Camera13","dataset3",
                                        "dataset4","dataset5","Door1","Door8","MattressCover2","Microphone1","Microphone3","Microphone4"
                                        ,"Microphone5","Microphone6","MonitorVideo7","SensorBracelet3","SensorBracelet4","Service3",
                                        "Service4","Tent1","VibrationAllarm3","VibrationAllarm4"};                                                                                

    vector<EddieResource *> resources = {};

    std::vector<std::string> allName;

    string base_path = DESCRIPTORS_DIR;


    if (program["--one"] == true)
        allName = allName1;
    if (program["--two"] == true)
        allName = allName2;
    if (program["--three"] == true)
        allName = allName3;                


    LOG_DBG("base_path:%s", base_path.c_str());
    if (program["--exampleres"] == true) 
    {
        for (std::string name : allName)
        {      
            ifstream ifs_alarm(base_path + "/json/config/" + name + ".json");
            Json::CharReaderBuilder builder;
            Json::Value root;
            builder["collectComments"] = true;
            builder["indentation"] = "";
            JSONCPP_STRING errs;
            if (!Json::parseFromStream(builder, ifs_alarm, &root, &errs)) {
                LOG_DBG("Error parsing json file: %s, %s", errs.c_str(), string(base_path + "/json/config/" + name + ".json").c_str());
                exit(-1);
            }


            if(name.find("ActuatorWater") != std::string::npos)
            {   
                resources.push_back(new EddieActuatorsWater(name,"",root,false));
            }     

            if(name.find("AllarmNoisy") != std::string::npos)
            {
                resources.push_back(new EddieAllarmNoisy(name,"",root,false));
            }     
            if(name.find("AllarmSilent") != std::string::npos)
            {
                resources.push_back(new EddieAllarmSilent(name,"",root,false));
            }     
            if(name.find("AudioSpeaker") != std::string::npos)
            {
                resources.push_back(new EddieAudioSpeaker(name,"",root,false));
            }     
            if(name.find("Bulb") != std::string::npos)
            {
                resources.push_back(new EddieBulb(name,"",root,false));
            }     
            if(name.find("CalculatorCPU") != std::string::npos)
            {
                resources.push_back(new EddieCalculatorCPU(name,"",root,false));
            }     
            if(name.find("Camera") != std::string::npos)
            {
                resources.push_back(new EddieCamera(name,"",root,false));
            }     
            if(name.find("dataset") != std::string::npos)
            {
                resources.push_back(new EddieDataset(name,"",root,false));
            }     
            if(name.find("Door") != std::string::npos)
            {
                resources.push_back(new EddieDoor(name,"",root,false));
            }     
            if(name.find("FacialRecognizer") != std::string::npos)
            {
                resources.push_back(new EddieFacialRecognizer(name,"",root,false));
            }     
            if(name.find("MattressCover") != std::string::npos)
            {
                resources.push_back(new EddieMattressCover(name,"",root,false));
            }     
            if(name.find("Microphone") != std::string::npos)
            {
                resources.push_back(new EddieMicrophone(name,"",root,false));
            }     
            if(name.find("MonitorVideo") != std::string::npos)
            {
                resources.push_back(new EddieMonitorVideo(name,"",root,false));
            }     
            if(name.find("SensorBracelet") != std::string::npos)
            {
                resources.push_back(new EddieSensorBracelet(name,"",root,false));
            }     
            if(name.find("Service") != std::string::npos)
            {
                resources.push_back(new EddieService(name,"",root,false));
            }     
            if(name.find("TemperatureSensor") != std::string::npos)
            {
                resources.push_back(new EddieTemperatureSensor(name,"",root,false));
            }     
            if(name.find("Tent") != std::string::npos)
            {
                resources.push_back(new EddieTent(name,"",root,false));
            }     
            if(name.find("VibrationAllarm") != std::string::npos)
            {
                resources.push_back(new EddieVibrationAllarm(name,"",root,false));
            }
            if(name.find("led_remote") != std::string::npos)
            {
                resources.push_back(new EddieBlinkingLamp("lamp_r","rt=eddie.r.blinking.lamp&ct=40",root,true));
            }     


        }
        
    }
    
    //Check Resources added in the vector
    for (int i = 0; i < resources.size(); i++)
    {
        LOG_DBG("%d: path=%s, Some attr=%s&%s&%s",i, *(resources[i]->get_path()), (resources[i]->get_attributes())[0],(resources[i]->get_attributes())[1],(resources[i]->get_attributes())[2]);
    }    

    receiver.run(resources);
}
