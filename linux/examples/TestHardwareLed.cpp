#include <thread>
#include <csignal>
#include <fstream>
#include "../hardware/include/GPIOWriter.h"
#include "../hardware/include/OneWireReader.h"
#include <iostream>

int main(int argc, char *argv[]) 
{
    GPIOWriter writer = GPIOWriter(27, true);     
    OneWireReader reader = OneWireReader("28-3c01f09582ff", true);

    while (true)
    {
        writer.write(1);
        std:: cout << "on" << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        writer.write(0);
        std:: cout << "off" << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));

        long temperature = reader.read_value();
        std:: cout << "temperature: "  << temperature << "\n";
        
    }
                               

    
}