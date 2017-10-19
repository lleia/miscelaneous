#include "dasmap.h"

using namespace dastrie;

int main(int argc,char *argv[])
{
    vector<string> word_list;
    word_list.push_back("eight");
    word_list.push_back("five");
    word_list.push_back("four");
    word_list.push_back("nine");

    vector<float> value_list;
    value_list.push_back(0.8);
    value_list.push_back(0.5);
    value_list.push_back(0.4);
    value_list.push_back(0.9);

    dasmap<float>::build(word_list,value_list,"map_sample.db");

    dasmap<float> map("map_sample.db");

    string key = "eight";
    float val;
    if (map.find(key,val))
        std::cout << key << " " << val << std::endl;

    key = "five";
    if (map.find(key,val))
        std::cout << key << " " << val << std::endl;

    key = "four";
    if (map.find(key,val))
        std::cout << key << " " << val << std::endl;

    key = "nine";
    if (map.find(key,val))
        std::cout << key << " " << val << std::endl;

    key = "one";
    if (map.find(key,val))
        std::cout << key << " " << val << std::endl;
 
    return 0;
}
