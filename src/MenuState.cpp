#include "MenuState.h"
#include "Qor/Input.h"
#include "Qor/Qor.h"
#include "Qor/TileMap.h"
#include "Qor/Sound.h"
#include "Qor/Sprite.h"
#include <glm/glm.hpp>
#include <cstdlib>
#include <chrono>
#include <thread>
#include "Qor/ScreenFader.h"
#include "Qor/IPartitioner.h"
#include "Qor/BasicPartitioner.h"
using namespace std;
using namespace glm;

MenuState :: MenuState(
    Qor* engine
    //string fn
):
    m_pQor(engine),
    m_pInput(engine->input()),
    //m_pInterpreter(engine->interpreter()),
    //m_pScript(make_shared<Interpreter::Context>(engine->interpreter())),
    m_pPipeline(engine->pipeline()),
    m_pResources(engine->resources()),
    m_pRoot(make_shared<Node>()),
    m_pCanvas(make_shared<Canvas>(
        engine->window()->size().x, engine->window()->size().y
    )),
    m_pMenuGUI(make_shared<MenuGUI>(
        engine->session()->profile(0)->controller().get(),
        &m_MenuContext,
        &m_MainMenu,
        m_pPipeline->partitioner(),
        m_pCanvas.get(),
        m_pResources,
        "Good Times",
        engine->window()->size().y / 30.0f,
        &m_Fade,
        6
    ))
{
}

void MenuState :: preload()
{
    m_pCamera = make_shared<Camera>(m_pResources, m_pQor->window());
    m_pRoot->add(m_pCamera);
    m_pCamera->listen(true);
    
    //try{
    //    m_pScene = m_pResources->cache_as<Scene>("menu.json");
    //}catch(const std::exception& e){
    //    ERRORf(GENERAL, "scene problem: %s", e.what());
    //}
    //m_pRoot->add(m_pScene->root());
    
    auto win = m_pQor->window();

    auto logo = make_shared<Mesh>(
        make_shared<MeshGeometry>(
            Prefab::quad(
                -vec2(win->size().y, win->size().y)/4.0f,
                vec2(win->size().y/4.0f, win->size().y/16.0f)
            )
        ));
    logo->add_modifier(make_shared<Wrap>(Prefab::quad_wrap(
        glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 0.0f)
    )));
    auto tex = m_pResources->cache_as<ITexture>("title.png");
    logo->material(make_shared<MeshMaterial>(tex));
    logo->move(vec3(
        win->center().x,
        win->center().y / 2.0f + win->size().y / 8.0f / 2.0f,
        -1.0f
    ));
    m_pRoot->add(logo);

    m_pMusic = m_pQor->make<Sound>("menu.ogg");
    m_pRoot->add(m_pMusic);
    m_Ambient.on_change.connect([this](const Color& c){
        int u = m_pPipeline->shader(1)->uniform("Ambient");
        if(u != -1)
            m_pPipeline->shader(1)->uniform(u, vec4(c.vec3(), c.a()));
    });
}

void MenuState :: enter()
{
    m_pCamera->ortho();
    m_pPipeline->winding(true);
    m_pPipeline->bg_color(Color::black());
    
    m_pInput->relative_mouse(false);
    if(m_pMusic)
    {
        //m_pMusic->source()->pitch = 0.0f;
        m_pMusic->source()->gain = 0.0f;
        m_pMusic->source()->refresh();
        m_pMusic->play();
    }
         
    float sw = m_pQor->window()->size().x;
    float sh = m_pQor->window()->size().y;
    
    m_pRoot->add(m_pCanvas);
    
    auto bg = make_shared<Mesh>(
        make_shared<MeshGeometry>(Prefab::quad(vec2(sw, sh))),
        vector<shared_ptr<IMeshModifier>>{
            make_shared<Wrap>(Prefab::quad_wrap())
        },
        make_shared<MeshMaterial>("sky3.png", m_pResources)
    );
    //auto bg2 = bg->instance();
    bg->position(vec3(0.0f,0.0f,-1.0f));
    //bg2->position(vec3(0.0f,0.0f,-1.0f));
    m_pRoot->add(bg);
    on_tick.connect([this, bg](Freq::Time t){
        m_WrapAccum.x += t.seconds() * 0.01f;
        m_WrapAccum.y += t.seconds() * 0.01f;
        
        m_WrapAccum += m_pInput->mouse_rel() * 0.0001f;
        m_WrapAccum.x = std::fmod(m_WrapAccum.x, 1.0f);
        m_WrapAccum.y = std::fmod(m_WrapAccum.y, 1.0f);
        
        bg->swap_modifier(0, make_shared<Wrap>(Prefab::quad_wrap(
             vec2(0.0f), vec2(1.0f),
             vec2(0.8f + 0.2f * m_Fade), //scale
             vec2(m_WrapAccum.x * 1.0f, m_WrapAccum.y * 1.0f) //offset
        )));
    });
    
    // set up screen fade
    m_pPipeline->blend(true);
    
    //m_MainMenu.name("Qorpse");
    m_MainMenu.options().emplace_back("Start Game", [this]{
        m_pDone = make_shared<std::function<void()>>([this]{
            m_pQor->change_state(m_pQor->states().class_id("game"));
        });
        m_pMenuGUI->pause();
    });
    //m_MainMenu.options().emplace_back("Continue", [this]{
    //    m_pDone = make_shared<std::function<void()>>([this]{
    //        m_pQor->change_state(m_pQor->states().class_id("game"));
    //    });
    //    m_pMenuGUI->pause();
    //});
    m_MainMenu.options().emplace_back("Options", [this]{
        m_MenuContext.push(&m_OptionsMenu);
    });
    m_MainMenu.options().emplace_back("Quit", [this]{ 
        m_pDone = make_shared<std::function<void()>>([this]{
            m_pQor->pop_state();
        });
    });
    
    m_pVolumeText = std::make_shared<string>(string("Global Volume: ") + to_string(
        m_pResources->config()->meta("audio")->at<int>("volume")
        ) + "%"
    );
    m_pSoundText = std::make_shared<string>(string("Sound Volume: ") + to_string(
        m_pResources->config()->meta("audio")->at<int>("sound-volume")
        ) + "%"
    );
    m_pMusicText = std::make_shared<string>(string("Music Volume: ") + to_string(
        m_pResources->config()->meta("audio")->at<int>("music-volume")
        ) + "%"
    );
    m_OptionsMenu.options().emplace_back(m_pVolumeText,
        [this]{
        },
        [this](int ofs){
            int old_v = m_pResources->config()->meta("audio")->at<int>("volume");
            int v = kit::clamp(old_v + ofs * 10, 0, 100);
            if(v!=old_v) {
                m_pResources->config()->meta("audio")->set<int>("volume", v);
                *m_pVolumeText = string("Global Volume: ") + to_string(v) + "%";
                Sound::play(m_pRoot.get(), "scroll.wav", m_pResources);
                return true;
            }
            return false;
        }
    );
    m_OptionsMenu.options().emplace_back(m_pMusicText,
        [this]{
            //Sound::play(m_pRoot.get(), "scroll.wav", m_pResources);
        },
        [this](int ofs){
            int old_v = m_pResources->config()->meta("audio")->at<int>("music-volume");
            int v = kit::clamp(old_v + ofs * 10, 0, 100);
            if(v!=old_v) {
                m_pResources->config()->meta("audio")->set<int>("music-volume", v);
                *m_pMusicText = string("Music Volume: ") + to_string(v) + "%";
                return true;
            }
            return false;
        });
    m_OptionsMenu.options().emplace_back(m_pSoundText,
        [this]{
        },
        [this](int ofs){
            int old_v = m_pResources->config()->meta("audio")->at<int>("sound-volume");
            int v = kit::clamp(old_v + ofs * 10, 0, 100);
            if(v!=old_v) {
                m_pResources->config()->meta("audio")->set<int>("sound-volume", v);
                *m_pSoundText = string("Sound Volume: ") + to_string(v) + "%";
                Sound::play(m_pRoot.get(), "scroll.wav", m_pResources);
                return true;
            }
            return false;
        });
    m_OptionsMenu.options().emplace_back("Customize Controls", [this]{
        m_MenuContext.push(&m_ControlsMenu);
    });
    m_OptionsMenu.options().emplace_back(
        "Back",
        [this]{
            m_pQor->save_settings();
            m_MenuContext.pop();
        },
        std::function<bool(int)>(), // no adjust
        string(), // no desc
        Menu::Option::BACK
    );

    init_controls_menu();

    m_MenuContext.clear(&m_MainMenu);
    m_pRoot->add(m_pMenuGUI);
    
    on_tick.connect(std::move(screen_fader(
        [this](Freq::Time, float fade) {
            m_Fade = fade;
            float v = m_pResources->config()->meta("audio")->at<int>("volume") / 100.0f;
            v *= m_pResources->config()->meta("audio")->at<int>("music-volume") / 100.0f;
            //m_pMusic->source()->pitch = fade;
            m_pMusic->source()->gain = fade * v;
            m_pMusic->source()->refresh();
            m_Ambient = Color(fade);
        },
        [this](Freq::Time){
            return !!m_pDone;
        },
        [this](Freq::Time){
            m_Ambient = Color::white();
            if(m_pDone)
                (*m_pDone)();
            else
                m_pQor->pop_state();
        }
    )));
    
    // set up gradual border animation
    {
        auto fade = make_shared<Animation<vec2>>();
    
        fade->stop(vec2(0.0f, 0.0f));
        fade->frame(Frame<vec2>(
            vec2(sw / 16.0f, sh / 16.0f),
            Freq::Time::seconds(2.0f),
            INTERPOLATE(out_sine<vec2>)
        ));
    }

    m_MenuContext.on_stack_empty.connect([this]{
        m_pDone = make_shared<std::function<void()>>([this]{
            m_pQor->pop_state();
        });
    });
}

MenuState :: ~MenuState()
{
    m_pPipeline->partitioner()->clear();
    m_pResources->optimize();
}

void MenuState :: init_controls_menu()
{
    shared_ptr<Meta> binds;
    TRY(binds = m_pQor->session()->profile(0)->config()->
        meta("input")->meta("binds")
    );

    if(binds)
    {
        for(auto&& bind: *binds)
        {
            try{
                // individual action -> key
                m_Binds[bind.as<string>()].push_back(bind.key);
            }catch(const boost::bad_any_cast&){
                // many actions -> one key
                auto bind_list = bind.as<shared_ptr<Meta>>();
                for(auto&& key: *bind_list)
                    m_Binds[key.as<string>()].push_back(bind.key);
            }
        }

        // TODO: add empty binds for buttons not found
        
        for(auto&& bind: m_Binds)
        {
            string action = bind.first;
            vector<string> keys = bind.second;
            auto text = make_shared<string>(
                boost::to_upper_copy(action) + ": " +
                boost::algorithm::join(keys, ", ")
            );
            m_ControlsMenu.options().emplace_back(
                text,
                [this, action, text]{
                    *text = action + ": ...";
                    m_pMenuGUI->pause();
                    m_pText = make_shared<string>();
                    auto gui = m_pMenuGUI;
                    auto guitext = m_pText;
                    m_pInput->listen(Input::LISTEN_KEY, m_pText,
                        [text,action,gui,guitext](bool done){
                            *text = action + ": " + *guitext;
                            if(done){
                                gui->pause(false);
                                gui->consume();
                            }
                        }
                    );
                }
            );
        }
    }

    m_ControlsMenu.options().emplace_back(
        "Back", 
        [this]{
            m_pQor->save_settings();
            m_MenuContext.pop();
        },
        std::function<bool(int)>(), // no adjust
        string(), // no desc
        Menu::Option::BACK
    );
}

void MenuState :: logic(Freq::Time t)
{
    Actuation::logic(t);
    m_pPipeline->logic(t);
    if(m_pInput->key(SDLK_F10))
        m_pQor->quit();

    m_pRoot->logic(t);
}

void MenuState :: render() const
{
    m_pPipeline->render(m_pRoot.get(), m_pCamera.get());
}

