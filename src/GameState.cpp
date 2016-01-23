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
    //m_pInterpreter(engine->interpreter()),
    //m_pScript(make_shared<Interpreter::Context>(engine->interpreter())),
    m_pPipeline(engine->pipeline())
{
}

GameState :: GameState(
    Qor* engine,
    std::string fn
):
    GameState(engine)
{
    m_Filename = fn;
}

void GameState :: preload()
{
    m_pCamera = make_shared<Camera>(m_pQor->resources(), m_pQor->window());
    m_pOrthoCamera = make_shared<Camera>(m_pQor->resources(), m_pQor->window());
    m_pRoot->add(m_pCamera->as_node());
    m_pOrthoRoot->add(m_pOrthoCamera->as_node());
    
    m_pMusic = m_pQor->make<Sound>("1.ogg");
    m_pRoot->add(m_pMusic);
    
    //m_pPipeline = make_shared<Pipeline>(
    //    m_pQor->window(),
    //    m_pQor->resources(),
    //    m_pRoot,
    //    m_pCamera
    //);
    m_pPhysics = make_shared<Physics>(m_pRoot.get(), (void*)this);
    
    //m_pRoot->add(make_shared<Mesh>(
    //    m_pQor->resource_path("level_silentScalpels.obj"),
    //    m_pQor->resources()
    //));
    m_pController = m_pQor->session()->profile(0)->controller();
    //m_pPlayer = kit::init_shared<PlayerInterface3D>(
    //    m_pController,
    //    m_pCamera,
    //    m_pQor->session()->profile(0)->config()
    //);
    
    auto win = m_pQor->window();
    auto bg = make_shared<Mesh>(
        make_shared<MeshGeometry>(
            Prefab::quad(
                -vec2(win->size().y, win->size().y),
                vec2(win->size().y, win->size().y)
            )
        )
    );
    bg->add_modifier(make_shared<Wrap>(Prefab::quad_wrap()));
    bg->material(make_shared<MeshMaterial>(
        m_pQor->resources()->cache_as<ITexture>(
            "sky1.png"
        )
    ));
    //bg->position(glm::vec3(0.0f, 0.0f, 0.0f));
    bg->position(glm::vec3(win->center().x, win->center().y, 0.0f));
    m_pOrthoRoot->add(bg);
    
    m_pShip = m_pQor->make<Mesh>("quickship.obj");
    m_pShip->set_physics(Node::DYNAMIC);
    m_pShip->set_physics_shape(Node::BOX);
    m_pShip->mass(1.0f);
    m_pShip->rotate(0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    m_pShip->position(glm::vec3(0.0f, 10.0f, 0.0f));
    LOGf("ship children: %s", m_pShip->num_children());
    m_pRoot->add(m_pShip);

    auto m = make_shared<Mesh>(
        m_pQor->resource_path("road1.obj"),
        m_pQor->resources()
    );

    m_pRoot->add(m);
    
    //m_pViewModel = make_shared<ViewModel>(
    //    m_pCamera,
    //    make_shared<Mesh>(
    //        m_pQor->resource_path("gun_bullpup.obj"),
    //        m_pQor->resources()
    //    )
    //);
    ////m_pViewModel->node()->rotate(0.5f, Axis::Z);
    //m_pViewModel->node()->position(glm::vec3(
    //    ads ? 0.0f : 0.05f,
    //    ads ? -0.04f : -0.06f,
    //    ads ? -0.05f : -0.15f
    //));
    //m_pRoot->add(m_pViewModel);
    m_pPhysics->generate(m_pRoot.get(), (unsigned)Physics::GenerateFlag::RECURSIVE);
}

GameState :: ~GameState()
{
    m_pPipeline->partitioner()->clear();
}

void GameState :: enter()
{
    m_pInput->relative_mouse(false);
    
    m_pPipeline->winding(false);
    m_pCamera->perspective();
    m_pCamera->mode(Camera::FOLLOW);
    m_pCamera->focus_time(Freq::Time(50));
    m_pCamera->focal_offset(glm::vec3(0.025f, 2.5f, 8.0f));
    m_pCamera->track(m_pShip);
    m_pOrthoCamera->ortho();
    
    m_pMusic->play();

    //m_pScene = make_shared<Scene>(
    //    m_pQor->resource_path("level_tantrum2013.json"),
    //    m_pQor->resources()
    //);
    //m_pRoot->add(m_pScene->root());
    
    on_tick.connect(std::move(screen_fader(
        [this](Freq::Time, float fade) {
            int fadev = m_pPipeline->shader(1)->uniform("LightAmbient");
            if(fadev != -1)
                m_pPipeline->shader(1)->uniform(
                    fadev,
                    glm::vec4(fade,fade,fade,1.0f)
                );
        },
        [this](Freq::Time){
            if(m_pInput->key(SDLK_ESCAPE))
                return true;
            return false;
        },
        [this](Freq::Time){
            m_pPipeline->shader(1)->uniform(
                m_pPipeline->shader(1)->uniform("LightAmbient"),
                Color::white().vec4()
            );
            m_pPipeline->blend(false);
            m_pQor->pop_state();
        }
    )));

    //m_pScript->execute_string("enter()");
}

void GameState :: logic(Freq::Time t)
{
    Actuation::logic(t);
    
    if(m_pInput->key(SDLK_ESCAPE))
        m_pQor->quit();
 
    //LOG(Vector::to_string(m_pCamera->position(Space::WORLD)));

    //if(m_pController->button("zoom").pressed_now())
    //    m_pViewModel->zoom(not m_pViewModel->zoomed());

    //m_pViewModel->sway(m_pPlayer->move() != glm::vec3(0.0f));
    //m_pViewModel->sprint(
    //    m_pPlayer->move() != glm::vec3(0.0f) && m_pPlayer->sprint()
    //);
    
    m_pPhysics->logic(t);
    
    //m_pScript->execute_string((
    //    boost::format("logic(%s)") % t.s()
    //).str());

    //float speed = 1000.0f * t.s();
    //if(m_pInput->key(SDLK_r))
    //{
    //    *m_pCamera->matrix() = glm::scale(
    //        *m_pCamera->matrix(), glm::vec3(1.0f-t.s(), 1.0f-t.s(), 1.0f)
    //    );
    //    m_pCamera->pend();
    //}
    
    //glm::vec3 v(0.0f);
    //v.z = old_v.z;
    btRigidBody* ship_body = (btRigidBody*)m_pShip->body()->body();
    glm::vec3 v = Physics::fromBulletVector(ship_body->getLinearVelocity());
    
    if(m_pInput->key(SDLK_e))
        v.z -= 15.0f * t.s();
    //    v.z -= 50.0f * t.s();
    //    //m_pShip->acceleration(glm::vec3(0.0f, 0.0f, 0.1f));
    //    //v.z = -5.0f;
    else if(m_pInput->key(SDLK_d))
        v.z += 15.0f * t.s();
    //    v.z += 50.0f * t.s();
    //    //v.z = 5.0f;
    ////else
    //    //m_pShip->acceleration(glm::vec3(0.0f));
    //v.z = std::min<float>(0.0f, std::max<float>(-25.0f, v.z));

    if(m_pInput->key(SDLK_s))
        v.x = -7.0f;
    else if(m_pInput->key(SDLK_f))
        v.x = 7.0f;
    else
        v.x = 0.0f;

    if(m_pInput->key(SDLK_SPACE))
        v.y = 5.0f;
        //rb->setLinearVelocity(btVector3(0.0, 5.0, 0.0));
    //    v.y = 10.0f;
    //else if(m_pShip->position().y > 0.0f)
    //    v.y = -10.0f;
    ////else if(m_pInput->key(SDLK_a))
    ////    v.y = -10.0f;
    
    ship_body->setLinearVelocity(Physics::toBulletVector(v));
    //m_pShip->velocity(v);
    
    //if(m_pInput->key(SDLK_UP))
    //    m_pCamera->move(glm::vec3(0.0f, -speed, 0.0f));
    //if(m_pInput->key(SDLK_DOWN))
    //    m_pCamera->move(glm::vec3(0.0f, speed, 0.0f));
    
    //if(m_pInput->key(SDLK_LEFT))
    //    m_pCamera->move(glm::vec3(-speed, 0.0f, 0.0f));
    //if(m_pInput->key(SDLK_RIGHT))
    //    m_pCamera->move(glm::vec3(speed, 0.0f, 0.0f));

    //LOGf("children: %s", m_pRoot->num_children());
    m_pOrthoRoot->logic(t);
    m_pRoot->logic(t);
}

void GameState :: render() const
{
    m_pPipeline->winding(true);
    m_pPipeline->render(
        m_pOrthoRoot.get(),
        m_pOrthoCamera.get(),
        nullptr,
        Pipeline::NO_DEPTH
    );
    m_pPipeline->winding(false);
    m_pPipeline->render(
        m_pRoot.get(),
        m_pCamera.get(),
        nullptr,
        Pipeline::NO_CLEAR
    );
}

