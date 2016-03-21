#include <glm/glm.hpp>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <string>
#include "GameState.h"
#include "Qor/Qor.h"
#include "Qor/kit/log/log.h"
#include "Qor/ScreenFader.h"
#include "Qor/Sound.h"
#include "Qor/Mesh.h"
#include "Qor/Sprite.h"
#include "Qor/PlayerInterface3D.h"
using namespace std;
using namespace glm;

GameState :: GameState(
    Qor* engine
    //std::string fn
):
    m_pQor(engine),
    m_pInput(engine->input()),
    m_pRoot(make_shared<Node>()),
    m_pOrthoRoot(make_shared<Node>()),
    m_pPipeline(engine->pipeline()),
    m_JumpTimer(&m_GameTime),
    m_Map(m_pQor->args().value_or("map", "road1"))
{
    m_Shader = m_pPipeline->load_shaders({"color"});
}

//GameState :: GameState(
//    Qor* engine,
//    std::string fn
//):
//    GameState(engine)
//{
//    // TODO: clear+clear cache of overrides, set size of shaders back to default
//    //m_pPipeline->override_shader(PassType::NORMAL, m_ColorShader);
//}

void GameState :: preload()
{
    m_pCamera = make_shared<Camera>(m_pQor->resources(), m_pQor->window());
    m_pCamera->fov(100);
    m_pOrthoCamera = make_shared<Camera>(m_pQor->resources(), m_pQor->window());
    m_pRoot->add(m_pCamera);
    m_pOrthoRoot->add(m_pOrthoCamera);
    
    m_pMusic = m_pQor->make<Sound>("2.ogg");
    m_pRoot->add(m_pMusic);
    
    m_pPhysics = make_shared<Physics>(m_pRoot.get(), (void*)this);
    m_pPhysics->world()->setGravity(btVector3(0.0, -60.0, 0.0));
    
    m_pController = m_pQor->session()->profile(0)->controller();
    
    auto win = m_pQor->window();
    auto bg = make_shared<Mesh>(
        make_shared<MeshGeometry>(
            Prefab::quad(
                -vec2(win->center().x, win->center().y),
                vec2(win->center().x, win->center().y)
            )
        )
    );
    bg->add_modifier(make_shared<Wrap>(Prefab::quad_wrap(
        vec2(1.0f, -1.0f)
    )));
    bg->material(make_shared<MeshMaterial>(
        m_pQor->resources()->cache_cast<ITexture>(
            "sky5.png"
        )
    ));
    bg->position(glm::vec3(win->center().x, win->center().y, 0.0f));
    m_pOrthoRoot->add(bg);
    
    m_pShip = m_pQor->make<Mesh>("quickship.obj");
    m_pShip->set_physics(Node::DYNAMIC);
    m_pShip->set_physics_shape(Node::BOX);
    m_pShip->mass(1.0f);
    m_pShip->inertia(false);
    m_pShip->rotate(0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    m_pShip->position(glm::vec3(0.0f, 10.0f, 0.0f));
    //LOGf("box: %s", m_pShip->box().string());
    m_pRoot->add(m_pShip);

    m_pMap = make_shared<Mesh>(
        m_pQor->resource_path(m_Map+".obj"),
        m_pQor->resources()
    );
    m_pMap->set_physics(Node::STATIC);

    m_pRoot->add(m_pMap);

    m_pPhysics->generate(m_pRoot.get(), Physics::GEN_RECURSIVE);

    btRigidBody* ship_body = (btRigidBody*)m_pShip->body()->body();
    ship_body->setFriction(0.0);
    ship_body->setCcdMotionThreshold(0.001f);
    ship_body->setCcdSweptSphereRadius(0.25f);
    ship_body->setActivationState(DISABLE_DEACTIVATION);

    m_sndLand = m_pQor->make<Sound>("land.wav");
    m_sndJump = m_pQor->make<Sound>("jump.wav");
    m_sndCrash = m_pQor->make<Sound>("crash.wav");
    m_sndFall = m_pQor->make<Sound>("fall.wav");
    m_sndFinish = m_pQor->make<Sound>("finish.wav");

    //LOG(m->box().string());
}

GameState :: ~GameState()
{
    //m_pMusic->stop();
    //m_pMusic = std::shared_ptr<Sound>();
    //m_pQor->resources()->optimize();
    m_pPipeline->partitioner()->clear();
}

void GameState :: enter()
{
    m_pInput->relative_mouse(false);
    
    m_pPipeline->winding(false);
    m_pCamera->perspective();
    m_pCamera->mode(Camera::FOLLOW);
    m_pCamera->focus_time(Freq::Time(50));
    m_pCamera->focal_offset(glm::vec3(0.025f, 2.0f, 6.0f));
    m_pCamera->track(m_pShip);
    m_pCamera->range(0.01f, 100.0f);
    m_pOrthoCamera->ortho();
    
    m_pMusic->play();

    //on_tick.connect(std::move(screen_fader(
    //    [this](Freq::Time, float fade) {
    //        int fadev = m_pPipeline->shader(1)->uniform("LightAmbient");
    //        if(fadev != -1)
    //            m_pPipeline->shader(1)->uniform(
    //                fadev,
    //                glm::vec4(fade,fade,fade,1.0f)
    //            );
    //    },
    //    [this](Freq::Time){
    //        if(m_pInput->key(SDLK_ESCAPE))
    //            return true;
    //        return false;
    //    },
    //    [this](Freq::Time){
    //        m_pPipeline->shader(1)->uniform(
    //            m_pPipeline->shader(1)->uniform("LightAmbient"),
    //            Color::white().vec4()
    //        );
    //        m_pPipeline->blend(false);
    //        m_pQor->pop_state();
    //    }
    //)));
}

void GameState :: logic(Freq::Time t)
{
    Actuation::logic(t);
    m_GameTime.logic(t);
    
    if(m_pInput->key(SDLK_ESCAPE).pressed_now()) {
        m_pQor->change_state("menu");
        return;
    }
 
    m_pPhysics->logic(t);

    if(m_pShip)
    {
        vec3 v = m_pShip->velocity();

        // raycast front and bottom for crashing and jumping
        auto pos = m_pShip->position();
        auto jump_hit = m_pPhysics->first_hit(
            pos,
            pos - m_pShip->box().size().y/2.0f - glm::vec3(0.0f, 1.0f, 0.0f)
        );
        auto front_hit = m_pPhysics->first_hit(
            pos,
            pos + glm::vec3(0.0f, 0.0f, -2.0f)
        );
        
        auto top_hit = m_pPhysics->first_hit(
            pos,
            pos - m_pShip->box().size().y/2.0f + glm::vec3(0.0f, 100.0f, 0.0f)
        );
        auto bottom_hit = m_pPhysics->first_hit(
            pos,
            pos - m_pShip->box().size().y/2.0f - glm::vec3(0.0f, 100.0f, 0.0f)
        );

        Node* jump_hit_node = std::get<0>(jump_hit);
        Node* front_hit_node = std::get<0>(front_hit);
        Node* top_hit_node = std::get<0>(top_hit);
        Node* bottom_hit_node = std::get<0>(bottom_hit);

        //LOGf("ship pos %s", m_pShip->position().z);
        //LOGf("map min %s", m_pShip->box().min().z);
        //LOGf("top hit %s", !!top_hit_node);
        //LOGf("bottom hit %s", !!bottom_hit_node);
        if(m_pShip->position().z <
            m_pMap->box().min().z + m_pShip->box().size().z &&
            top_hit_node && bottom_hit_node
        ){
            m_sndFinish->position(m_pShip->position());
            m_pRoot->add(m_sndFinish);
            m_pShip->clear_body();
            m_pShip->detach();
            m_pShip = nullptr;
            m_sndFinish->on_done([this]{
                m_pQor->change_state("menu");
            });
            m_sndFinish->play();
            auto profile = m_pQor->session()->profile(0);
            auto profcfg = profile->config();
            auto progress = profcfg->ensure("progress", make_shared<Meta>());
            progress->set<bool>(m_Map, true);
            //profile->sync(); // doesn't work yet
            return;
        }

        //assert(jump_hit_node != m_pShip.get());
        //assert(front_hit_node != m_pShip.get());
        if(jump_hit_node == m_pShip.get())
            jump_hit_node = nullptr;
        if(front_hit_node == m_pShip.get())
            front_hit_node = nullptr;
        glm::vec3 front_hit_normal = std::get<2>(front_hit);
        
        // ship crashing
        if(front_hit_node && v.z < -20.0f &&
           front_hit_normal == glm::vec3(0.0f, 0.0f, 1.0f)
        ){
            //LOG(Vector::to_string(front_hit_normal));
            m_pShip->add(m_sndCrash);
            m_sndCrash->play();
            reset();
            return;
        }
        //assert(front_hit_node != m_pShip.get());
        
        if(m_pController->button("accelerate"))
            v.z -= 15.0f * t.s();
        else if(m_pController->button("decelerate"))
            v.z += 15.0f * t.s();
        
        v.z = std::max(v.z, -35.0f);

        if(m_pController->button("left"))
            v.x = -5.0f;
            //v.x -= 14.0f * t.s();
        else if(m_pController->button("right"))
            v.x = 5.0f;
            //v.x += 14.0f * t.s();
        else
            v.x = 0.0f;
        
        v.x = std::max(std::min(v.x, 10.0f), -10.0f);
        
        if(jump_hit_node && m_bInAir)
        {
            m_pShip->add(m_sndLand);
            m_sndLand->play();
        }
        m_bInAir = not jump_hit_node;
        if(m_pController->button("jump"))
        {
            if(jump_hit_node)
            {
                m_pShip->add(m_sndJump);
                m_sndJump->play();
                m_JumpTimer.set(Freq::Time(150));
                v.y = 15.0f;
            }
            else if(not m_JumpTimer.elapsed())
            {
                v.y = 15.0f;
            }
        } else {
            m_JumpTimer.reset();
        }
        
        m_pShip->velocity(v);
        //m_pCamera->fov(60.0f + 30.0f * std::min(1.0f, std::max(0.0f, std::abs(v.z/15.0f))));
        
        if(m_pShip->position().y < m_pMap->box().min().y)
        {
            m_pShip->add(m_sndFall);
            m_sndFall->play();
            reset();
        }
    }

    m_pOrthoRoot->logic(t);
    m_pRoot->logic(t);
}

void GameState :: reset()
{
    m_pShip->position(glm::vec3(0.0f, 10.0f, 0.0f));
    m_pShip->velocity(vec3(0.0f));
}

void GameState :: render() const
{
    m_pPipeline->override_shader(PassType::NORMAL, (unsigned)PassType::NONE);
    m_pPipeline->winding(true);
    m_pPipeline->render(
        m_pOrthoRoot.get(),
        m_pOrthoCamera.get(),
        nullptr,
        Pipeline::NO_DEPTH
    );
    m_pPipeline->override_shader(PassType::NORMAL, m_Shader);
    m_pPipeline->winding(false);
    m_pPipeline->render(
        m_pRoot.get(),
        m_pCamera.get(),
        nullptr,
        Pipeline::NO_CLEAR
    );
    m_pPipeline->override_shader(PassType::NORMAL, (unsigned)PassType::NONE);
}

