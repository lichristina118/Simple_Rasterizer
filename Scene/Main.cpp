#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <limits>
#include <string.h>
#include <iostream>

#include <cpplocate/cpplocate.h>


//#include "SceneDemoApp.hpp"
#include "SceneApp.hpp"


#if defined(_WIN32)
#  include <conio.h>
#  include <windows.h>
#endif

int main(int argc, char *argv[])  {

    int opt;
    std::string skybox_name = "rainbow";
    while((opt = getopt(argc, argv, "hs:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'h':  
                printf("Rasterization input formats:\n");
                printf("   Scene -s [skybox_name] [obj_or_dae_file] [scene_info_file]\n");
                printf("\n");
                printf("\t-s specifies the skybox name.\n");
                printf("\t   Skybox files are located under the Scene/skybox_files directory.\n");
                printf("\tThe default input file is bunnyscene.dae in the current directory.\n");
                printf("\tThe default scene info file is bunnyscene_info.json in the current directory.\n");  
                printf("\n");
                printf("\tA GUI window will display the scene. Use mouse to change the camera position.\n"); 
                printf("\t    Press c to toggle between default camera and built-in camera.\n"); 
                printf("\t    Press d to toggle between deferred and forward rendering.\n");
                printf("\t    Press e to toggle the skybox.\n"); 
                printf("\t    For forward rendering, press f to toggle the flat shader.\n"); 
                printf("\t    For deferred rendering:\n");
                printf("\t\t  Press g to toggle between displaying g-buffers and scene.\n");
                printf("\t\t  Press s to toggle sunsky model\n");
                printf("\t\t  Press b to toggle blur pass.\n"); 
                exit(0); 
            case 's':
                skybox_name = optarg;
                break;
        }  
    }
 
    // parse the arguments
    const std::string resourcePath = cpplocate::locatePath("resources/scenes", "", nullptr) + "resources/scenes/";
    std::string input_file, info_file;
    int argCount = argc - optind;
    if (argCount > 0) {
        input_file = argv[optind++];
    } else {
        input_file = "bunnyscene.dae";            
    }
    if (argCount > 1) {
        info_file = argv[optind++];
    } else {
        info_file = "bunnyscene_info.json";
    }
    input_file = resourcePath + input_file;
    info_file = resourcePath + info_file;

    nanogui::init();

    printf("The input file name is: %s\n", input_file.c_str());
    printf("The scene info file name is: %s\n", info_file.c_str());
    printf("The skybox files are named: %s\n", skybox_name.c_str());
    nanogui::ref<SceneApp> app = new SceneApp(input_file, info_file, skybox_name);
    nanogui::mainloop(16);        
    nanogui::shutdown();
}
