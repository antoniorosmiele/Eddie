/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include <thread>
#include <unordered_map>

#include "VirtualizationSender.h"
#include "../communication/include/common.h"
#include "eddie.h"
#include "common.h"
#include <bits/stdc++.h>

#define N_Allarm_Noisy 3
#define N_Temperature_Sensors 1
#define N_Allarm_Silent 1
#define N_Actuators_Water 1

std::vector<Link> filterConstraint(std::vector<Link> * links);
std::vector<std::string> createConstraint(std::vector<Link> * links);


int main() {

    struct timeval tv;
    struct timezone tz;
    long double start_time;
    long double stop_time;

    guint watcher_id = connect();

    std::thread main([]() {
        run();
    });

    Json::Value h_c;
    h_c = Json::arrayValue;
    h_c[0]["rt"] = "eddie.r.allarm.noisy";
    //h_c[0]["rt"] = "eddie.r.fplace";

    auto message = compose_message("find", "", h_c, {});
    auto answer = send_message(message);

    auto res_link = parse_link_format(answer);

    h_c[0]["rt"] = "eddie.r.temperature";
    auto message1 = compose_message("find", "", h_c, {});
    auto answer1 = send_message(message1);

    auto res_link1 = parse_link_format(answer1);

    for (size_t i = 0; i < res_link1.size(); i++)
        res_link.push_back(res_link1[i]);

    h_c[0]["rt"] = "eddie.r.allarm.silent";
    message1 = compose_message("find", "", h_c, {});
    answer1 = send_message(message1);

    res_link1 = parse_link_format(answer1);

    for (size_t i = 0; i < res_link1.size(); i++)
        res_link.push_back(res_link1[i]);

    h_c[0]["rt"] = "eddie.r.actuator.water";
    message1 = compose_message("find", "", h_c, {});
    answer1 = send_message(message1);

    res_link1 = parse_link_format(answer1);

    for (size_t i = 0; i < res_link1.size(); i++)
        res_link.push_back(res_link1[i]);
    

    LOG_DBG("RESOURCES FOUND:");

    LOG_DBG("Res:%ld",res_link.size());

    for (const auto& res: res_link) {
        LOG_DBG("%s:%s@%s", res.host.c_str(), res.port.c_str(), res.path.c_str());
    }
    
    //Creation constraint
    gettimeofday(&tv, &tz);

    start_time = tv.tv_sec*1000 + tv.tv_usec * 0.001;

    auto res_link_filtered = filterConstraint(&res_link);
    std::vector<std::string> constr = createConstraint(&res_link_filtered);

    gettimeofday(&tv, &tz);

    stop_time = tv.tv_sec*1000 + tv.tv_usec * 0.001;

    LOG_DBG("time constraints: %Lf", stop_time-start_time);

    LOG_DBG("RESOURCES FILTERED:");
    for (const auto& res: res_link_filtered) 
    {
        LOG_DBG("%s:%s@%s", res.host.c_str(), res.port.c_str(), res.path.c_str());
    }
    
    LOG_DBG("CONSTRAINTS CREATED:");

    for (const auto& con: constr) 
    {
        LOG_DBG("%s", con.c_str());
    }

    Json::Value h_cs, o_f;

    h_cs = Json::arrayValue;
    o_f = Json::arrayValue;

    h_cs[0]["size"] = std::to_string( res_link_filtered.size());

    int i = 1;

    for (auto &link : res_link_filtered)
    {
        h_cs[i]["dst_ip"] = link.host;
        h_cs[i]["dst_port"] = link.port;
        h_cs[i]["dst_res"] = link.path;

        std::map<std::string, std::string> *attrs = &(link.attrs);

        for (auto &e : *attrs)
        {
           h_cs[i][e.first] = e.second;
           LOG_DBG("Resource:%s %s=%s",link.path.c_str(),e.first.c_str(),e.second.c_str());
        }
        i++;                
    }

    //Constraints

    o_f[0]["max/min"] = MGM_MAX;

    for (int i = 0; i < constr.size(); i++)
    {
        o_f[i+1]["constr" + std::to_string(i+1)] = constr[i];
    }
    
    //...........

    //auto messageS = compose_message("selection", "", h_cs, o_f);

    //auto answerS = send_message(messageS);
    gettimeofday(&tv, &tz);

    start_time = tv.tv_sec*1000 + tv.tv_usec * 0.001;

    auto answerS = selection(h_cs,o_f, res_link_filtered.size());

    gettimeofday(&tv, &tz);

    stop_time = tv.tv_sec*1000 + tv.tv_usec * 0.001;

    LOG_DBG("time selection: %Lf", stop_time-start_time);

    LOG_DBG("%s", answerS.c_str());

    answerS = answerS.substr(1);
    answerS.pop_back();

    std::replace( answerS.begin(), answerS.end(), ',', ' ');

    LOG_DBG("%s", answerS.c_str());

    std::stringstream lineStream(answerS);
 
    std::vector<int> vectorValues(std::istream_iterator<int>(lineStream),{});
    
    i = 0;

    LOG_DBG("Resource Selected:");

    for (auto res : res_link_filtered)
    {
        LOG_DBG("%d=%d", i ,vectorValues[i]);
        if (vectorValues[i] == 1)
        {
            LOG_DBG("%s:%s@%s", res.host.c_str(), res.port.c_str(), res.path.c_str());
        }
        
        i++;
    }
    

    disconnect(watcher_id);
    main.join();
}

std::vector<Link> filterConstraint(std::vector<Link> * links)
{
    std::vector<Link> newLinks;
    for (auto &link : *links)
    {
        std::map<std::string, std::string> *attrs = &(link.attrs);

        std::string rt =attrs->find("rt")->second;

        //apply constraint
        if (rt == "eddie.r.allarm.silent")
        {   
            if (attrs->find("owner")->second == "Security")
                newLinks.push_back(link);
        }
        else
        {
            newLinks.push_back(link);
        }
        
        

    }   

    return newLinks; 
}

std::vector<std::string> createConstraint(std::vector<Link> * links)
{
    std::vector<std::string> constraints;

    //Create constraints of cardinality

    std::string k = "";

    int i = 0;

    for (auto &link : *links)
    {
        std::map<std::string, std::string> *attrs = &(link.attrs);

        std::string rt =attrs->find("rt")->second;

        if (rt == "eddie.r.allarm.noisy")
        {
            k+= " x" + std::to_string(i) + " +";
        }
        
        i++;
    }

    if(k.back() == '+') k.pop_back();

    k+= "= " + std::to_string(N_Allarm_Noisy);

    constraints.push_back(k);

    k = "";

    i = 0;

    for (auto &link : *links)
    {
        std::map<std::string, std::string> *attrs = &(link.attrs);

        std::string rt =attrs->find("rt")->second;

        if (rt == "eddie.r.temperature")
        {
            k+= " x" + std::to_string(i) + " +";
        }
        
        i++;
    }

    if(k.back() == '+') k.pop_back();

    k+= "= " + std::to_string(N_Temperature_Sensors);

    constraints.push_back(k);    

    k = "";

    i = 0;

    for (auto &link : *links)
    {
        std::map<std::string, std::string> *attrs = &(link.attrs);

        std::string rt =attrs->find("rt")->second;

        if (rt == "eddie.r.allarm.silent")
        {
            k+= " x" + std::to_string(i) + " +";
        }
        
        i++;
    }

    if(k.back() == '+') k.pop_back();

    k+= "= " + std::to_string(N_Allarm_Silent);

    constraints.push_back(k);

    k = "";

    i = 0;

    for (auto &link : *links)
    {
        std::map<std::string, std::string> *attrs = &(link.attrs);

        std::string rt =attrs->find("rt")->second;

        if (rt == "eddie.r.actuator.water")
        {
            k+= " x" + std::to_string(i) + " +";
        }
        
        i++;
    }

    if(k.back() == '+') k.pop_back();

    k+= "= " + std::to_string(N_Actuators_Water);

    constraints.push_back(k);

    //Create constraints of max or min
    std::string max = "";

    i = 0;

    for (auto &link : *links)
    {
        std::map<std::string, std::string> *attrs = &(link.attrs);

        std::string rt =attrs->find("rt")->second;

        if (rt == "eddie.r.temperature")
        {
            max+= " x" + std::to_string(i) + " * " + attrs->find("accuracy")->second + " +";
        }
        
        i++;
    }

    if(max.back() == '+') max.pop_back();

    max.pop_back();

    constraints.push_back(max);

    //Create constraints of gain cardinality (Only for cardinality > 1)

    std::string gc = "";

    i = 0;

    for (auto &link : *links)
    {
        std::map<std::string, std::string> *attrs = &(link.attrs);

        std::string rt =attrs->find("rt")->second;

        if (rt == "eddie.r.allarm.noisy")
        {
            gc+= " x" + std::to_string(i) + " +";
        }
        
        i++;
    }

    if(gc.back() == '+') gc.pop_back();

    gc.pop_back();

    constraints.push_back(gc);        

    //Create constraints of comparison

    std::string cmp = "";

    i= 0;
    
    for (auto &link1 : *links)
    {
        std::map<std::string, std::string> *attrs1 = &(link1.attrs);

        std::string rt1 =attrs1->find("rt")->second;

        if (rt1 == "eddie.r.actuator.water")
        {   
            int j = 0;

            for (auto &link2 : *links)
            {
                std::map<std::string, std::string> *attrs2 = &(link2.attrs);

                std::string rt2 =attrs2->find("rt")->second;

                if (rt2 == "eddie.r.temperature")
                {
                    cmp+= " x" + std::to_string(i) + " * x" + std::to_string(j) + " * strcmp(" + attrs1->find("place")->second  + "," + attrs2->find("place")->second + ") +";
                }

                j++;             
            }
        }

        i++;     
    }

    if(cmp.back() == '+') cmp.pop_back();

    cmp+= "= " + std::to_string(N_Actuators_Water);

    constraints.push_back(cmp);

    return constraints;

}