#include <iostream>
#include <memory>
#include <thread>
#include "Qor/kit/kit.h"
#include "Qor/Qor.h"
#include "Info.h"
#include "GameState.h"
#include "MenuState.h"

#include "Qor/kit/log/log.h"
#include "Qor/kit/async/async.h"

#ifdef DEBUG
    #include <backward/backward.cpp>
#endif

using namespace std;
using namespace kit;

int main(int argc, const char** argv)
{
    
    Args args(argc, argv);
    args.set("mod","bouncingship");
    args.set("title","Bouncing Ship");
    //args.set("basic_shader","fog");
    //args.set("no_loading_fade","true");
    
    Texture::DEFAULT_FLAGS = Texture::TRANSPARENT | Texture::FILTER | Texture::MIPMAP;
    
#ifndef DEBUG
    try{
#endif
        auto engine = kit::make_unique<Qor>(args);
        engine->states().register_class<GameState>("game");
        engine->states().register_class<MenuState>("menu");
        engine->run("game");
#ifndef DEBUG
    }catch(const Error&){
        // already logged
    }catch(const std::exception& e){
        LOGf("Uncaught exception: %s", e.what());
    }
#endif
    return 0;
}

