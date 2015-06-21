/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2015 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lvl_player.h"

#include <graphics/window.h>
#include <graphics/gl_renderer.h>
#include <data_configs/config_manager.h>
#include <audio/pge_audio.h>
#include <audio/SdlMusPlayer.h>

#include <common_features/rectf.h>
#include <common_features/pointf.h>
#include <common_features/maths.h>

#include "lvl_scene_ptr.h"

#include <QtDebug>

#include <fontman/font_manager.h>

//#define COLLIDE_DEBUG

LVL_Player::LVL_Player() : PGE_Phys_Object()
{
    camera = NULL;

    playerID = 0;
    isLocked = false;
    _no_render = false;
    _isInited = false;
    isLuaPlayer = false;

    isExiting = false;

    _direction = 1;

    type = LVLPlayer;

    frameW=100;
    frameH=100;

    stateID     =1;
    characterID =1;

    _heightDelta=0;

    _doSafeSwitchCharacter=false;

    _stucked=false;

    JumpPressed=false;
    onGround=false;
    on_slippery_surface=false;
    forceCollideCenter=false;

    climbing=false;

    collide_player = COLLISION_ANY;
    collide_npc    = COLLISION_NONE;

    environment = LVL_PhysEnv::Env_Air;
    last_environment = LVL_PhysEnv::Env_Air;

    bumpDown=false;
    bumpUp=false;
    bumpVelocity=0.0f;

    bumpJumpVelocity=0.0f;
    bumpJumpTime=0.0f;

    _exiting_swimTimer=700;

    health=3;
    doHarm=false;
    doHarm_damage=0;

    jumpTime=0;
    jumpVelocity=5.3f;

    isAlive = true;
    doKill = false;
    kill_reason=DEAD_fall;

    _isRunning = false;

    contactedWithWarp = false;
    contactedWarp = NULL;
    wasEnteredTimeout = 0;
    wasEntered = false;
    warpDirectO = 0;
    isWarping=false;
    warpPipeOffset=0;

    gscale_Backup=0.f;//!< BackUP of last gravity scale

    duck_allow=false;
    ducking=false;

    /********************Attack************************/
    attack_enabled=true;
    attack_pressed=false;
    /********************Attack************************/

    /********************floating************************/
    floating_allow=false;
    floating_maxtime=1000; //!< Max time to float
    floating_isworks=false;                         //!< Is character currently floating in air
    floating_timer=0;                            //!< Milliseconds to float
    floating_start_type=false;
    /********************floating************************/
}

LVL_Player::~LVL_Player()
{}

void LVL_Player::setCharacter(int CharacterID, int _stateID)
{
    #ifdef COLLIDE_DEBUG
    qDebug() << "==================TOGGLE=CHARACTER=============================";
    #endif
    if(!_doSafeSwitchCharacter)
    {
        if(!ConfigManager::playable_characters.contains(CharacterID))
        {
            PGE_Audio::playSoundByRole(obj_sound_role::PlayerSpring);
            return;
        }
        else
            setup = ConfigManager::playable_characters[CharacterID];
        states =   ConfigManager::playable_characters[CharacterID].states;
        if(!states.contains(_stateID))
        {
            PGE_Audio::playSoundByRole(obj_sound_role::CameraSwitch);
            return;
        }
        else
            state_cur = states[_stateID];
    }
    else _doSafeSwitchCharacter=false;

    physics = setup.phys_default;
    physics_cur = physics[environment];

    long tID = ConfigManager::getLvlPlayerTexture(CharacterID, _stateID);
    if( tID >= 0 )
    {
        texId = ConfigManager::level_textures[tID].texture;
        texture = ConfigManager::level_textures[tID];
        frameW = ConfigManager::level_textures[tID].w / setup.matrix_width;
        frameH = ConfigManager::level_textures[tID].h / setup.matrix_height;
    }

    animator.setSize(setup.matrix_width, setup.matrix_height);
    animator.installAnimationSet(state_cur.sprite_setup);

    phys_setup.max_vel_x = fabs(_isRunning ?
                physics_cur.MaxSpeed_run :
                physics_cur.MaxSpeed_walk);
    phys_setup.min_vel_x = -fabs(_isRunning ?
                physics_cur.MaxSpeed_run :
                physics_cur.MaxSpeed_walk);
    phys_setup.max_vel_y = fabs(physics_cur.MaxSpeed_down);
    phys_setup.min_vel_y = -fabs(physics_cur.MaxSpeed_up);
    phys_setup.decelerate_x = physics_cur.decelerate_air;
    phys_setup.gravityScale = physics_cur.gravity_scale;
    phys_setup.gravityAccel = physics_cur.gravity_accel;
    phys_setup.grd_dec_x    = physics_cur.walk_force;

    /********************floating************************/
    floating_allow=state_cur.allow_floating;
    floating_maxtime=state_cur.floating_max_time; //!< Max time to float
    if(_isInited)
    {
        if(!floating_allow && floating_isworks)
            floating_timer=0;
    }
    /********************floating************************/

    /********************duck**************************/
    duck_allow= state_cur.duck_allow;
    /********************duck**************************/

    characterID = CharacterID;
    stateID = _stateID;

    if(_isInited)
    {
        ducking = (ducking & state_cur.duck_allow);
        float cx = posRect.center().x();
        float b = posRect.bottom();
        setSize(state_cur.width, ducking?state_cur.duck_height:state_cur.height);
        setPos(cx-_width/2, b-_height);
        PlayerState x = LvlSceneP::s->getGameState()->getPlayerState(playerID);
        x.characterID    = characterID;
        x.stateID        = stateID;
        x._chsetup.state = stateID;
        LvlSceneP::s->getGameState()->setPlayerState(playerID, x);
        _collideUnduck();

        #ifdef COLLIDE_DEBUG
        //Freeze time to check out what happening at moment
        //GlRenderer::makeShot();
        //LvlSceneP::s->isTimeStopped=true;
        #endif
    }
}

void LVL_Player::setCharacterSafe(int CharacterID, int _stateID)
{
    if(!ConfigManager::playable_characters.contains(CharacterID))
    {
        PGE_Audio::playSoundByRole(obj_sound_role::PlayerSpring);
        return;
    }
    else
        setup = ConfigManager::playable_characters[CharacterID];
    states =   ConfigManager::playable_characters[CharacterID].states;

    float oldHeight = posRect.height();

    if(!states.contains(_stateID))
    {
        PGE_Audio::playSoundByRole(obj_sound_role::CameraSwitch);
        return;
    }
    else
        state_cur = states[_stateID];
    _doSafeSwitchCharacter=true;

    _heightDelta=oldHeight-( (ducking&&state_cur.duck_allow) ? state_cur.duck_height : state_cur.height);
    if(_heightDelta>0.0f) _heightDelta=0.0f;

    characterID = CharacterID;
    stateID = _stateID;
}

void LVL_Player::setPlayerPointInfo(PlayerPoint pt)
{
    data = pt;
    playerID=pt.id;
    PlayerState x = LvlSceneP::s->getGameState()->getPlayerState(playerID);
    characterID = x.characterID;
    stateID     = x._chsetup.state;
    if(_isInited) setCharacter(characterID, stateID);
}

void LVL_Player::setDuck(bool duck)
{
    if(!duck_allow) return;
    if(duck==ducking) return;
    float b=posRect.bottom();
    setSize(state_cur.width, duck? state_cur.duck_height : state_cur.height);
    setPos(posX(), b-_height);
    ducking=duck;
    if(!duck && !isWarping)
    {
        _heightDelta = state_cur.duck_height-state_cur.height;
        if(_heightDelta>0.0f) _heightDelta=0.0f;
        _collideUnduck();
    }
}

void LVL_Player::_collideUnduck()
{
    _syncPosition();
    if(isWarping) return;

    #ifdef COLLIDE_DEBUG
    qDebug() << "do unduck!";
    #endif

    //Detect collidable blocks!
    QVector<PGE_Phys_Object*> bodies;
    PGE_RectF posRectC = posRect;
    posRectC.setTop(posRectC.top()-4.0f);
    posRectC.setLeft(posRectC.left()+1.0f);
    posRectC.setRight(posRectC.right()-1.0f);
    posRectC.setBottom(posRectC.bottom()+ (ducking ? 0.0:(-posRect.height()/2.0))+4.0 );
    LvlSceneP::s->queryItems(posRectC, &bodies);

    forceCollideCenter=true;
    for(PGE_RenderList::iterator it=bodies.begin();it!=bodies.end(); it++ )
    {
        PGE_Phys_Object*body=*it;
        if(body==this) continue;
        if(body->isPaused()) continue;
        solveCollision(body);
    }
    forceCollideCenter=false;

    #ifdef COLLIDE_DEBUG
    int hited=0;
    PGE_RectF nearestRect;
    #endif

    bool resolveTop=false;
    double _floorY=0;
    QVector<LVL_Block*> blocks_to_hit;

    if((!collided_center.isEmpty())&&(collided_bottom.isEmpty()))
    {
        for(PlayerColliders::iterator it=collided_center.begin(); it!=collided_center.end() ; it++)
        {
            PGE_Phys_Object *collided= *it;
            LVL_Block *blk= static_cast<LVL_Block*>(collided);
            if(blk) blocks_to_hit.push_back(blk);
        }
        for(PlayerColliders::iterator it=collided_top.begin(); it!=collided_top.end() ; it++)
        {
            PGE_Phys_Object *collided= *it;
            LVL_Block *blk= static_cast<LVL_Block*>(collided);
            if(blk) blocks_to_hit.push_back(blk);
        }
        if(isFloor(blocks_to_hit))
        {
            LVL_Block*nearest = nearestBlockY(blocks_to_hit);
            if(nearest)
            {
                _floorY=nearest->posRect.bottom()+1.0;
                resolveTop=true;
            }
        }
    }

    if(resolveTop)
    {
        posRect.setY(_floorY);
        if(!blocks_to_hit.isEmpty())
        {
            LVL_Block*nearest = nearestBlock(blocks_to_hit);
            if(nearest)
            {
                resolveTop=true;
                long npcid=nearest->data.npc_id;
                nearest->hit();
                if( nearest->setup->hitable || (npcid!=0) || (nearest->destroyed) || (nearest->setup->bounce) )
                {
                    bump(false, speedY());
                }
            }
        }
        setSpeedY(0.0);
        _syncPosition();
    }

    _heightDelta = 0.0;
    #ifdef COLLIDE_DEBUG
    qDebug() << "unduck: blocks to hit"<< hited << " Nearest: "<<nearestRect.top() << nearestRect.bottom() << "are bottom empty: " << collided_bottom.isEmpty() << "bodies found: "<<bodies.size() << "bodies at center: "<<collided_center.size();
    #endif
}

void LVL_Player::init()
{
    setCharacter(characterID, stateID);

    _direction = data.direction;
    long posX = data.x+(data.w/2)-(state_cur.width/2);
    long posY = data.y = data.y+data.h-state_cur.height;
    setSize(state_cur.width, state_cur.height);
    setPos(posX, posY);
    phys_setup.max_vel_y=12;
    animator.tickAnimation(0.f);
    isLocked=false;
    _isInited=true;

    _syncSection();
}

void LVL_Player::update(float ticks)
{
    if(isLocked) return;
    if(!_isInited) return;
    if(!camera) return;
    LVL_Section* section = sct();
    if(!section) return;

    event_queue.processEvents(ticks);

    if((isWarping) || (!isAlive))
    {
        animator.tickAnimation(ticks);
        updateCamera();
        return;
    }

    onGround = !foot_contacts_map.isEmpty();
    on_slippery_surface = !foot_sl_contacts_map.isEmpty();
    bool climbableUp  = !climbable_map.isEmpty();
    bool climbableDown= climbableUp && !onGround;
    climbing = (climbableUp && climbing && !onGround && (posRect.center().y()>=(climbableHeight-physics_cur.velocity_climb_y_up)) );
    if(onGround)
    {
        phys_setup.decelerate_x =
                (fabs(speedX())<=physics_cur.MaxSpeed_walk)?
                (on_slippery_surface?physics_cur.decelerate_stop/physics_cur.slippery_c : physics_cur.decelerate_stop):
                (on_slippery_surface?physics_cur.decelerate_run/physics_cur.slippery_c : physics_cur.decelerate_run);

        if(physics_cur.strict_max_speed_on_ground)
        {
            if((speedX()>0)&&(speedX()>phys_setup.max_vel_x))
                setSpeedX(phys_setup.max_vel_x);
            else
            if((speedX()<0)&&(speedX()<phys_setup.min_vel_x))
                setSpeedX(phys_setup.min_vel_x);
        }
    }
    else
        phys_setup.decelerate_x = physics_cur.decelerate_air;

    if(doKill)
    {
        doKill=false;
        isAlive = false;
        setPaused(true);
        LvlSceneP::s->checkPlayers();
        return;
    }

    if(climbing)
    {
        if(gscale_Backup != 1)
        {
            setGravityScale(0);
            gscale_Backup = 1;
        }
    }
    else
    {
        if(gscale_Backup != 0.f)
        {
            setGravityScale(physics_cur.gravity_scale);
            gscale_Backup = 0.f;
        }
    }

    if(climbing)
    {
        setSpeed(0,0);
    }

    if(environments_map.isEmpty())
    {
        if(last_environment!=section->getPhysicalEnvironment() )
        {
            environment = section->getPhysicalEnvironment();
        }
    }
    else
    {
        int newEnv = section->getPhysicalEnvironment();

        foreach(int x, environments_map)
        {
            newEnv = x;
        }

        if(last_environment != newEnv)
        {
            qDebug()<<"Enter to environment" << newEnv;
            environment = newEnv;
        }
    }

    if((last_environment!=environment)||(last_environment==-1))
    {
        physics_cur = physics[environment];
        last_environment=environment;

        if(physics_cur.zero_speed_y_on_enter)
        {
            if(speedY()>0)
                setSpeedY(0.0);
            else
                setSpeedY(speedY()*(physics_cur.slow_up_speed_y_coeff));
        }

        if(physics_cur.slow_speed_x_on_enter)
            setSpeedX( speedX()*(physics_cur.slow_speed_x_coeff) );

        if(JumpPressed) jumpVelocity=physics_cur.velocity_jump;

        phys_setup.max_vel_x = fabs(_isRunning ?
                    physics_cur.MaxSpeed_run :
                    physics_cur.MaxSpeed_walk) *(onGround?physics_cur.ground_c_max:1.0f);
        phys_setup.min_vel_x = -fabs(_isRunning ?
                    physics_cur.MaxSpeed_run :
                    physics_cur.MaxSpeed_walk) *(onGround?physics_cur.ground_c_max:1.0f);
        phys_setup.max_vel_y = fabs(physics_cur.MaxSpeed_down);
        phys_setup.min_vel_y = -fabs(physics_cur.MaxSpeed_up);
        phys_setup.decelerate_x = physics_cur.decelerate_air;
        phys_setup.gravityScale = physics_cur.gravity_scale;
        phys_setup.grd_dec_x    = physics_cur.walk_force;

        floating_isworks=false;//< Reset floating on re-entering into another physical envirinment
    }

    if(onGround)
    {
        if(!floating_isworks)
        {
            floating_timer=floating_maxtime;
        }
    }

    //Running key
    if(keys.run)
    {
        if(!_isRunning)
        {
            phys_setup.max_vel_x = physics_cur.MaxSpeed_run;
            phys_setup.min_vel_x = -physics_cur.MaxSpeed_run;
            _isRunning=true;
        }
    }
    else
    {
        if(_isRunning)
        {
            phys_setup.max_vel_x = physics_cur.MaxSpeed_walk;
            phys_setup.min_vel_x = -physics_cur.MaxSpeed_walk;
            _isRunning=false;
        }
    }
    if((physics_cur.ground_c_max!=1.0f))
    {
        phys_setup.max_vel_x = fabs(_isRunning ?
                    physics_cur.MaxSpeed_run :
                    physics_cur.MaxSpeed_walk) *(onGround?physics_cur.ground_c_max:1.0f);
        phys_setup.min_vel_x = -fabs(_isRunning ?
                    physics_cur.MaxSpeed_run :
                    physics_cur.MaxSpeed_walk) *(onGround?physics_cur.ground_c_max:1.0f);
    }


    if(keys.alt_run)
    {
        if(attack_enabled && !attack_pressed && !climbing)
        {
            attack_pressed=true;

            if(keys.up)
                attack(Attack_Up);
            else
            if(keys.down)
                attack(Attack_Down);
            else
            {
                attack(Attack_Forward);
                PGE_Audio::playSoundByRole(obj_sound_role::PlayerTail);
                animator.playOnce(MatrixAnimator::RacoonTail, _direction, 75, true, true, 1);
            }
        }
    }
    else
    {
        if(attack_pressed) attack_pressed=false;
    }



    //if
    if(!keys.up && !keys.down && !keys.left && !keys.right)
    {
        if(wasEntered)
        {
            wasEntered = false;
            wasEnteredTimeout=0;
        }
    }

    //Reset state
    if(wasEntered)
    {
        wasEnteredTimeout-=ticks;
        if(wasEnteredTimeout<0)
        {
            wasEnteredTimeout=0;
            wasEntered=false;
        }
    }

    if(keys.up)
    {
        if(climbableUp&&(jumpTime<=0))
        {
            setDuck(false);
            climbing=true;
            floating_isworks=false;//!< Reset floating on climbing start
        }

        if(climbing)
        {
            if(posRect.center().y() >= climbableHeight)
                setSpeedY(-physics_cur.velocity_climb_y_up);
        }
    }

    if(keys.down)
    {
        if( climbableDown && (jumpTime<=0) )
        {
            setDuck(false);
            climbing=true;
            floating_isworks=false;//!< Reset floating on climbing start
        }
        else
        {
            if((duck_allow & !ducking)&&( (animator.curAnimation()!=MatrixAnimator::RacoonTail) ) )
            {
                setDuck(true);
            }
        }

        if(climbing)
        {
            setSpeedY(physics_cur.velocity_climb_y_down);
        }
    }
    else
    {
        if(ducking)
            setDuck(false);
    }

    if( (!keys.left) || (!keys.right) )
    {
        bool turning=(((speedX()>0)&&(_direction<0))||((speedX()<0)&&(_direction>0)));

        float force = turning?
                    physics_cur.decelerate_turn :
                    (fabs(speedX())>physics_cur.MaxSpeed_walk)?physics_cur.run_force : physics_cur.walk_force;

        if(on_slippery_surface) force=force/physics_cur.slippery_c;
        else if((onGround)&&(physics_cur.ground_c!=1.0f)) force=force*physics_cur.ground_c;

        if(keys.left) _direction=-1;
        if(keys.right) _direction=1;

        if(!ducking || !onGround)
        {
            //If left key is pressed
            if(keys.right && collided_right.isEmpty())
            {
                if(climbing)
                    setSpeedX(physics_cur.velocity_climb_x);
                else
                    applyAccel(force, 0);
            }
            //If right key is pressed
            if(keys.left && collided_left.isEmpty())
            {
                if(climbing)
                    setSpeedX(-physics_cur.velocity_climb_x);
                else
                    applyAccel(-force, 0);
            }
        }
    }

    if( keys.alt_jump )
    {
        //Temporary it is ability to fly up!
        if(!bumpDown && !bumpUp) {
            setSpeedY(-physics_cur.velocity_jump);
        }
    }

    if( keys.jump )
    {
        if(!JumpPressed)
        {
            if(environment!=LVL_PhysEnv::Env_Water)
                { if(climbing || onGround || (environment==LVL_PhysEnv::Env_Quicksand))
                    PGE_Audio::playSoundByRole(obj_sound_role::PlayerJump); }
            else
                PGE_Audio::playSoundByRole(obj_sound_role::PlayerWaterSwim);
        }

        if((environment==LVL_PhysEnv::Env_Water)||(environment==LVL_PhysEnv::Env_Quicksand))
        {
            if(!JumpPressed)
            {
                if(environment==LVL_PhysEnv::Env_Water)
                {
                    if(!ducking) animator.playOnce(MatrixAnimator::SwimUp, _direction, 75);
                }
                else
                if(environment==LVL_PhysEnv::Env_Quicksand)
                {
                    if(!ducking) animator.playOnce(MatrixAnimator::JumpFloat, _direction, 64);
                }

                JumpPressed=true;
                jumpTime = physics_cur.jump_time;
                jumpVelocity=physics_cur.velocity_jump;
                floating_timer = floating_maxtime;
                setSpeedY(speedY()-jumpVelocity);
            }
        }
        else
        if(!JumpPressed)
        {
            JumpPressed=true;
            if(onGround || climbing)
            {
                climbing=false;
                jumpTime=physics_cur.jump_time;
                jumpVelocity=physics_cur.velocity_jump;
                floating_timer = floating_maxtime;
                setSpeedY(-jumpVelocity-fabs(speedX()/physics_cur.velocity_jump_c));
            }
            else
            if((floating_allow)&&(floating_timer>0))
            {
                floating_isworks=true;

                //if true - do floating with sin, if false - do with cos.
                floating_start_type=(speedY()<0);

                setSpeedY(0);
                setGravityScale(0);
            }
        }
        else
        {
            if(jumpTime>0)
            {
                jumpTime -= ticks;
                setSpeedY(-jumpVelocity-fabs(speedX()/physics_cur.velocity_jump_c));
            }

            if(floating_isworks)
            {
                floating_timer -= ticks;
                if(floating_start_type)
                    setSpeedY( state_cur.floating_amplitude*(-cos(floating_timer/80.0)) );
                else
                    setSpeedY( state_cur.floating_amplitude*(cos(floating_timer/80.0)) );
                if(floating_timer<=0)
                {
                    floating_timer=0;
                    floating_isworks=false;
                    setGravityScale(climbing?0:physics_cur.gravity_scale);
                }
            }
        }
    }
    else
    {
        jumpTime=0;
        if(JumpPressed)
        {
            JumpPressed=false;
            if(floating_allow)
            {
                if(floating_isworks)
                {
                    floating_timer=0;
                    floating_isworks=false;
                    setGravityScale(climbing?0:physics_cur.gravity_scale);
                }
            }
        }
    }

    refreshAnimation();
    animator.tickAnimation(ticks);

    PGE_RectF sBox = section->sectionLimitBox();

    //Return player to start position on fall down
    if( posY() > sBox.bottom()+_height )
    {
        kill(DEAD_fall);
    }

    if(bumpDown)
    {
        bumpDown=false;
        jumpTime=0;
        setSpeedY(bumpVelocity);
    }
    else
    if(bumpUp)
    {
        bumpUp=false;
        if(keys.jump)
        {
            jumpTime=bumpJumpTime;
            jumpVelocity=bumpJumpVelocity;
        }
        setSpeedY( (keys.jump ?
                        (-fabs(bumpJumpVelocity)-fabs(speedX()/physics_cur.velocity_jump_c)):
                         -fabs(bumpJumpVelocity)) );
    }


    //Connection of section opposite sides
    if(isExiting) // Allow walk offscreen if exiting
    {
        if(posX() < sBox.left()-_width-1 )
            setGravityScale(0);//Prevent falling [we anyway exited from this level, isn't it?]
        else
        if(posX() > sBox.right() + 1 )
            setGravityScale(0);//Prevent falling [we anyway exited from this level, isn't it?]

        if(keys.left||keys.right)
        {
            if((environment==LVL_PhysEnv::Env_Water)||(environment==LVL_PhysEnv::Env_Quicksand))
            {
                keys.run=true;
                if(_exiting_swimTimer<0 && !keys.jump)
                    keys.jump=true;
                else
                if(_exiting_swimTimer<0 && keys.jump)
                {
                    keys.jump=false; _exiting_swimTimer=(environment==LVL_PhysEnv::Env_Quicksand)? 1 : 500;
                }
                _exiting_swimTimer-= ticks;
            } else keys.run=false;
        }
    }
    else
    if(section->isWarp())
    {
        if(posX() < sBox.left()-_width-1 )
            setPosX( sBox.right()+1 );
        else
        if(posX() > sBox.right() + 1 )
            setPosX( sBox.left()-_width-1 );
    }
    else
    {

        if(section->ExitOffscreen())
        {
            if(section->RightOnly())
            {
                if( posX() < sBox.left())
                {
                    setPosX( sBox.left() );
                    setSpeedX(0.0);
                }
            }

            if((posX() < sBox.left()-_width-1 ) || (posX() > sBox.right() + 1 ))
            {
                setLocked(true);
                _no_render=true;
                LvlSceneP::s->setExiting(1000, LvlExit::EXIT_OffScreen);
                return;
            }
        }
        else
        {
            //Prevent moving of player away from screen
            if( posX() < sBox.left())
            {
                setPosX(sBox.left());
                setSpeedX(0.0);
            }
            else
            if( posX()+_width > sBox.right())
            {
                setPosX(sBox.right()-_width);
                setSpeedX(0.0);
            }
        }
    }

    if(_stucked)
    {
        posRect.setX(posRect.x()-_direction*2);
        applyAccel(0, 0);
    }

    if(contactedWithWarp)
    {
        if(contactedWarp)
        {

            switch( contactedWarp->data.type )
            {
            case 1://pipe
                {
                    bool isContacted=false;
               // Entrance direction: [3] down, [1] up, [2] left, [4] right

                    switch(contactedWarp->data.idirect)
                    {
                        case 4://right
                            if(posRect.right() >= contactedWarp->right()-1.0 &&
                               posRect.right() <= contactedWarp->right()+speedX() &&
                               posRect.bottom() >= contactedWarp->bottom()-1.0 &&
                               posRect.bottom() <= contactedWarp->bottom()+32.0  ) isContacted = true;
                            break;
                        case 3://down
                            if(posRect.bottom() >= contactedWarp->bottom()-1.0 &&
                               posRect.bottom() <= contactedWarp->bottom()+speedY() &&
                               posRect.center().x() >= contactedWarp->left() &&
                               posRect.center().x() <= contactedWarp->right()
                                    ) isContacted = true;
                            break;
                        case 2://left
                            if(posRect.left() <= contactedWarp->left()+1.0 &&
                               posRect.left() >= contactedWarp->left()+speedX() &&
                               posRect.bottom() >= contactedWarp->bottom()-1.0 &&
                               posRect.bottom() <= contactedWarp->bottom()+32.0  ) isContacted = true;
                            break;
                        case 1://up
                            if(posRect.top() <= contactedWarp->top()+1.0 &&
                               posRect.top() >= contactedWarp->top()+speedY() &&
                               posRect.center().x() >= contactedWarp->left() &&
                               posRect.center().x() <= contactedWarp->right()  ) isContacted = true;
                            break;
                        default:
                            break;
                    }

                    if(isContacted)
                    {
                        bool doTeleport=false;
                        switch(contactedWarp->data.idirect)
                        {
                            case 4://right
                                if(keys.right && !wasEntered) { setPosX(contactedWarp->right()-posRect.width()); doTeleport=true; }
                                break;
                            case 3://down
                                if(keys.down && !wasEntered) { setPosY(contactedWarp->bottom()-state_cur.height);
                                    animator.switchAnimation(MatrixAnimator::PipeUpDown, _direction, 115);
                                    doTeleport=true;}
                                break;
                            case 2://left
                                if(keys.left && !wasEntered) { setPosX(contactedWarp->left()); doTeleport=true; }
                                break;
                            case 1://up
                                if(keys.up && !wasEntered) {  setPosY(contactedWarp->top());
                                    animator.switchAnimation(MatrixAnimator::PipeUpDown, _direction, 115);
                                    doTeleport=true;}
                                break;
                            default:
                                break;
                        }

                        if(doTeleport)
                        {
                            WarpTo(contactedWarp->data);
                            wasEntered = true;
                            wasEnteredTimeout=800;
                        }
                    }

                }

                break;
            case 2://door
                if(keys.up && !wasEntered)
                {
                    bool isContacted=false;

                    if(posRect.bottom() <= contactedWarp->bottom()+fabs(speedY()) &&
                       posRect.bottom() >= contactedWarp->bottom()-fabs(speedY())-4.0 &&
                       posRect.center().x() >= contactedWarp->left() &&
                       posRect.center().x() <= contactedWarp->right()  ) isContacted = true;

                    if(isContacted)
                    {
                        setPosY(contactedWarp->bottom()-posRect.height());
                        WarpTo(contactedWarp->data);
                        wasEntered = true;
                        wasEnteredTimeout=800;
                    }
                }
                break;
            case 0://Instant
            default:
                if(!wasEntered)
                {
                    WarpTo(contactedWarp->data.ox, contactedWarp->data.oy, contactedWarp->data.type);
                    wasEnteredTimeout=800;
                    wasEntered = true;
                }
                break;
            }
        }
    }

    if(_doSafeSwitchCharacter) setCharacter(characterID, stateID);

    try {
        lua_onLoop();
    } catch (luabind::error& e) {
        LvlSceneP::s->getLuaEngine()->postLateShutdownError(e);
    }
    updateCamera();
}

void LVL_Player::updateCamera()
{
    camera->setCenterPos( round(posCenterX()), round(bottom())-state_cur.height/2 );
}

void LVL_Player::updateCollisions()
{
    foot_contacts_map.clear();
    onGround=false;
    foot_sl_contacts_map.clear();
    contactedWarp = NULL;
    contactedWithWarp=false;
    climbable_map.clear();
    environments_map.clear();

    collided_top.clear();
    collided_left.clear();
    collided_right.clear();
    collided_bottom.clear();
    collided_center.clear();

    #ifdef COLLIDE_DEBUG
    qDebug() << "=====Collision check and resolve begin======";
    #endif

    PGE_Phys_Object::updateCollisions();

    bool resolveBottom=false;
    bool resolveTop=false;
    bool resolveLeft=false;
    bool resolveRight=false;

    double backupX=posRect.x();
    double backupY=posRect.y();
    double _wallX=posRect.x();
    double _floorY=posRect.y();

    QVector<LVL_Block*> floor_blocks;
    QVector<LVL_Block*> wall_blocks;
    QVector<LVL_Block*> blocks_to_hit;
    if(!collided_bottom.isEmpty())
    {
        for(PlayerColliders::iterator it=collided_bottom.begin(); it!=collided_bottom.end() ; it++)
        {
            PGE_Phys_Object *collided= *it;
            switch(collided->type)
            {
                case PGE_Phys_Object::LVLBlock:
                {
                    LVL_Block *blk= static_cast<LVL_Block*>(collided);
                    if(!blk) continue;
                    foot_contacts_map[(intptr_t)collided]=(intptr_t)collided;
                    if(blk->slippery_surface) foot_sl_contacts_map[(intptr_t)collided]=(intptr_t)collided;
                    if(blk->setup->bounce) blocks_to_hit.push_back(blk);
                    floor_blocks.push_back(blk);
                }
                break;
                default:break;
            }
        }
        if(isFloor(floor_blocks))
        {
            LVL_Block*nearest = nearestBlockY(floor_blocks);
            if(nearest)
            {
                _floorY = nearest->posRect.top()-posRect.height();
                resolveBottom=true;
            }
        }
        else
        {
            foot_contacts_map.clear();
            foot_sl_contacts_map.clear();
        }
    }

    if(!collided_top.isEmpty())
    {
        blocks_to_hit.clear();
        for(PlayerColliders::iterator it=collided_top.begin(); it!=collided_top.end() ; it++)
        {
            PGE_Phys_Object *collided= *it;
            LVL_Block *blk= static_cast<LVL_Block*>(collided);
            if(blk) blocks_to_hit.push_back(blk);
            if(blk) floor_blocks.push_back(blk);
        }
        if(isFloor(floor_blocks))
        {
            LVL_Block*nearest = nearestBlockY(blocks_to_hit);
            if(nearest)
            {
                if(!resolveBottom) _floorY = nearest->posRect.bottom()+1;
                resolveTop=true;
            }
        }
    }

    bool wall=false;
    if(!collided_left.isEmpty())
    {
        for(PlayerColliders::iterator it=collided_left.begin(); it!=collided_left.end() ; it++)
        {
            PGE_Phys_Object *collided= *it;
            LVL_Block *blk= static_cast<LVL_Block*>(collided);
            if(blk) wall_blocks.push_back(blk);
        }
        if(isWall(wall_blocks))
        {
            LVL_Block*nearest = nearestBlock(wall_blocks);
            if(nearest)
            {
                _wallX = nearest->posRect.right();
                resolveLeft=true;
                wall=true;
            }
        }
    }

    if(!collided_right.isEmpty())
    {
        wall_blocks.clear();
        for(PlayerColliders::iterator it=collided_right.begin(); it!=collided_right.end() ; it++)
        {
            PGE_Phys_Object *collided= *it;
            LVL_Block *blk= static_cast<LVL_Block*>(collided);
            if(blk) wall_blocks.push_back(blk);
        }
        if(isWall(wall_blocks))
        {
            LVL_Block*nearest = nearestBlock(wall_blocks);
            if(nearest)
            {
                _wallX = nearest->posRect.left()-posRect.width();
                resolveRight=true;
                wall=true;
            }
        }
    }

    if((resolveLeft||resolveRight) && (resolveTop||resolveBottom))
    {
        //check if on floor or in air
        bool _iswall=false;
        bool _isfloor=false;
        posRect.setX(_wallX);
        _isfloor=isFloor(floor_blocks);
        posRect.setPos(backupX, _floorY);
        _iswall=isWall(wall_blocks);
        posRect.setX(backupX);
        if(!_iswall && _isfloor)
        {
            resolveLeft=false;
            resolveRight=false;
        }
        if(!_isfloor && _iswall)
        {
            resolveTop=false;
            resolveBottom=false;
        }
    }

    if(resolveLeft || resolveRight)
    {
        posRect.setX(_wallX);
        setSpeedX(0);
    }
    if(resolveBottom || resolveTop)
    {
        posRect.setY(_floorY);
        float bumpSpeed=speedY();
        setSpeedY(0);
        if(!blocks_to_hit.isEmpty())
        {
            LVL_Block*nearest = nearestBlock(blocks_to_hit);
            if(nearest)
            {
                long npcid=nearest->data.npc_id;
                if(resolveBottom) nearest->hit(LVL_Block::down); else nearest->hit();
                if( nearest->setup->hitable || (npcid!=0) || (nearest->destroyed) || nearest->setup->bounce )
                    bump(resolveBottom,
                         (resolveBottom ? physics_cur.velocity_jump_bounce : bumpSpeed),
                         physics_cur.jump_time_bounce);
            }
        }
        if(resolveTop && !bumpUp && !bumpDown )
            jumpTime=0;
    }
    else
    {
        posRect.setY(backupY);
    }
    _stucked = ( (!collided_center.isEmpty()) && (!collided_bottom.isEmpty()) && (!wall) );

    #ifdef COLLIDE_DEBUG
    qDebug() << "=====Collision check and resolve end======";
    #endif
}


void LVL_Player::solveCollision(PGE_Phys_Object *collided)
{
    if(!collided) return;

    switch(collided->type)
    {
        case PGE_Phys_Object::LVLBlock:
        {

            #ifdef COLLIDE_DEBUG
            qDebug() << "Player:"<<posRect.x()<<posRect.y()<<posRect.width()<<posRect.height();
            #endif
            LVL_Block *blk= static_cast<LVL_Block*>(collided);
            if(blk)
            {
                if(blk->destroyed)
                {
                    #ifdef COLLIDE_DEBUG
                    qDebug() << "Destroyed!";
                    #endif
                    break;
                }
            }
            else
            {
                #ifdef COLLIDE_DEBUG
                qDebug() << "Wrong cast";
                #endif
                break;
            }

            if( ((!forceCollideCenter)&&(!collided->posRect.collideRect(posRect)))||
                ((forceCollideCenter)&&(!collided->posRect.collideRectDeep(posRect, 1.0, -3.0))) )
            {
                #ifdef COLLIDE_DEBUG
                qDebug() << "No, is not collidng";
                #endif
                break;
            }
            #ifdef COLLIDE_DEBUG
            qDebug() << "Collided item! "<<collided->type<<" " <<collided->posRect.center().x()<<collided->posRect.center().y();
            #endif

            if((bumpUp||bumpDown)&&(!forceCollideCenter))
            {
                #ifdef COLLIDE_DEBUG
                qDebug() << "Bump? U'r dumb!";
                #endif
                break;
            }

            PGE_PointF c1 = posRect.center();
            PGE_RectF &r1 = posRect;
            PGE_PointF cc = collided->posRect.center();
            PGE_RectF  rc = collided->posRect;

            switch(collided->collide_player)
            {
                case COLLISION_TOP:
                {
                    PGE_RectF &r1=posRect;
                    PGE_RectF  rc = collided->posRect;
                    if(
                            (
                                (speedY() >= 0.0)
                                &&
                                (r1.bottom() < rc.top()+_velocityY_prev)
                                &&
                                (
                                     (r1.left()<rc.right()-1 ) &&
                                     (r1.right()>rc.left()+1 )
                                 )
                             )
                            ||
                            (r1.bottom() <= rc.top())
                            )
                    {
                        if(blk->isHidden) break;
                        collided_bottom[(intptr_t)collided]=collided;//bottom of player
                        #ifdef COLLIDE_DEBUG
                        qDebug() << "Top of block";
                        #endif
                    }
                }
                break;
                case COLLISION_ANY:
                {
                    #ifdef COLLIDE_DEBUG
                    bool found=false;
                    #endif
                    double xSpeed = Maths::max(fabs(speedX()), fabs(_velocityX_prev)) * Maths::sgn(speedX());
                    double ySpeed = Maths::max(fabs(speedY()), fabs(_velocityY_prev)) * Maths::sgn(speedY());
                    //*****************************Feet of player****************************/
                    if(
                            (
                                (speedY() >= 0.0)
                                &&
                                (floor(r1.bottom()) < rc.top()+ySpeed+fabs(speedX())+1.0)
                                &&( !( (r1.left()>=rc.right()-0.2) || (r1.right() <= rc.left()+0.2) ) )
                             )
                            ||
                            (r1.bottom() <= rc.top())
                            )
                    {
                            if(blk->isHidden) break;
                            collided_bottom[(intptr_t)collided]=collided;//bottom of player
                            #ifdef COLLIDE_DEBUG
                            qDebug() << "Top of block";
                            found=true;
                            #endif
                    }
                    //*****************************Head of player****************************/
                    else if( (
                                 (  ((!forceCollideCenter)&&(speedY()<0.0))||(forceCollideCenter&&(speedY()<=0.0))   )
                                 &&
                                 (r1.top() > rc.bottom()+ySpeed-1.0+_heightDelta)
                                 &&( !( (r1.left()>=rc.right()-0.5 ) || (r1.right() <= rc.left()+0.5 ) ) )
                              )
                             )
                    {
                        collided_top[(intptr_t)collided]=collided;//top of player
                        #ifdef COLLIDE_DEBUG
                        qDebug() << "Bottom of block";
                        found=true;
                        #endif
                    }
                    //*****************************Left****************************/
                    else if( (speedX()<0.0) && (c1.x() > cc.x()) && (r1.left() >= rc.right()+xSpeed-1.0)
                             && ( (r1.top()<rc.bottom())&&(r1.bottom()>rc.top()) ) )
                    {
                        if(blk->isHidden) break;
                        collided_left[(intptr_t)collided]=collided;//right of player
                        #ifdef COLLIDE_DEBUG
                        qDebug() << "Right of block";
                        #endif
                    }
                    //*****************************Right****************************/
                    else if( (speedX()>0.0) && (c1.x() < cc.x()) && ( r1.right() <= rc.left()+xSpeed+1.0)
                             && ( (r1.top()<rc.bottom())&&(r1.bottom()>rc.top()) ) )
                    {
                        if(blk->isHidden) break;
                        collided_right[(intptr_t)collided]=collided;//left of player
                        #ifdef COLLIDE_DEBUG
                        qDebug() << "Left of block";
                        found=true;
                        #endif
                    }


                    float c=forceCollideCenter? 0.0f : 1.0f;
                    //*****************************Center****************************/
                    #ifdef COLLIDE_DEBUG
                    qDebug() << "block" <<posRect.top()<<":"<<blk->posRect.bottom()
                             << "block" <<posRect.bottom()<<":"<<blk->posRect.top()<<" collide?"<<
                                blk->posRect.collideRectDeep(posRect,
                                                                                     fabs(_velocityX_prev)*c+c*2.0,
                                                                                     fabs(_velocityY_prev)*c+c*2.0) <<
                                "depths: "<< fabs(_velocityX_prev)*c+c*2.0 <<
                            fabs(_velocityY_prev)*c+c;
                    #endif
                    if( blk->posRect.collideRectDeep(posRect,
                                                     fabs(_velocityX_prev)*c+c*2.0,
                                                     fabs(_velocityY_prev)*c+c*2.0)
                            )
                    {
                        if(blk->isHidden && !forceCollideCenter) break;
                        collided_center[(intptr_t)collided]=collided;
                        #ifdef COLLIDE_DEBUG
                        qDebug() << "Center of block";
                        found=true;
                        #endif
                    }

                    #ifdef COLLIDE_DEBUG
                    qDebug() << "---checked---" << (found?"and found!": "but nothing..." )<<
                                r1.left()<< "<="<< rc.right()<<"+"<<xSpeed ;
                    #endif
                    break;
                }
            default: break;
            }
            break;
        }
        case PGE_Phys_Object::LVLWarp:
        {
            contactedWarp = static_cast<LVL_Warp*>(collided);
            if(contactedWarp)
                contactedWithWarp=true;
            break;
        }
        case PGE_Phys_Object::LVLBGO:
        {
            LVL_Bgo *bgo= static_cast<LVL_Bgo*>(collided);
            if(bgo)
            {
                if(bgo->setup->climbing)
                {
                    bool set=climbable_map.isEmpty();
                    climbable_map[(intptr_t)collided]=(intptr_t)collided;
                    if(set)
                        climbableHeight=collided->posRect.top();
                    else if(collided->top()<climbableHeight)
                        climbableHeight=collided->top();
                }
            }
            break;
        }
        case PGE_Phys_Object::LVLNPC:
        {
            LVL_Npc *npc= static_cast<LVL_Npc*>(collided);
            if(npc)
            {
                if(npc->isKilled())        break;
                if(npc->data.friendly) break;
                if(npc->setup->climbable)
                {
                    bool set=climbable_map.isEmpty();
                    climbable_map[(intptr_t)collided]=(intptr_t)collided;
                    if(set)
                        climbableHeight=collided->posRect.top();
                    else if(collided->top()<climbableHeight)
                        climbableHeight=collided->top();
                }

                if((!npc->data.friendly)&&(npc->setup->takable))
                {
                    npc->harm();
                    kill_npc(npc, LVL_Player::NPC_Taked_Coin);
                }

            }
            break;
        }
        case PGE_Phys_Object::LVLPhysEnv:
        {
            LVL_PhysEnv *env= static_cast<LVL_PhysEnv*>(collided);
            if(env)
            {
                if(env) environments_map[(intptr_t)env]=env->env_type;
            }
            break;
        }
    default: break;
    }
}

Uint32 slideTicks=0;
void LVL_Player::refreshAnimation()
{
    /**********************************Animation switcher**********************************/
        if(climbing)
        {
            animator.unlock();
            if((speedY()<0.0)||(speedX()!=0.0))
                animator.switchAnimation(MatrixAnimator::Climbing, _direction, 128);
            else
                animator.switchAnimation(MatrixAnimator::Climbing, _direction, -1);
        }
        else
        if(ducking)
        {
            animator.switchAnimation(MatrixAnimator::SitDown, _direction, 128);
        }
        else
        if(!onGround)
        {
            if(environment==LVL_PhysEnv::Env_Water)
            {
                if(speedY()>=0)
                    animator.switchAnimation(MatrixAnimator::Swim, _direction, 128);
                else
                    animator.switchAnimation(MatrixAnimator::SwimUp, _direction, 128);
            }
            else if(environment==LVL_PhysEnv::Env_Quicksand)
            {
                if(speedY()<0)
                    animator.switchAnimation(MatrixAnimator::JumpFloat, _direction, 64);
                else if(speedY()>0)
                    animator.switchAnimation(MatrixAnimator::Idle, _direction, 64);
            }
            else
            {
                if(speedY()<0)
                    animator.switchAnimation(MatrixAnimator::JumpFloat, _direction, 64);
                else if(speedY()>0)
                    animator.switchAnimation(MatrixAnimator::JumpFall, _direction, 64);
            }
        }
        else
        {
            bool busy=false;
            if((speedX()<-1)&&(_direction>0))
                if(keys.right)
                {
                    if(SDL_GetTicks()-slideTicks>100)
                    {
                        PGE_Audio::playSoundByRole(obj_sound_role::PlayerSlide);
                        slideTicks=SDL_GetTicks();
                    }
                    animator.switchAnimation(MatrixAnimator::Sliding, _direction, 64);
                    busy=true;
                }

            if(!busy)
            {
                if((speedX()>1)&&(_direction<0))
                    if(keys.left)
                    {
                        if(SDL_GetTicks()-slideTicks>100)
                        {
                            PGE_Audio::playSoundByRole(obj_sound_role::PlayerSlide);
                            slideTicks=SDL_GetTicks();
                        }
                        animator.switchAnimation(MatrixAnimator::Sliding, _direction, 64);
                        busy=true;
                    }
            }

            if(!busy)
            {
                float velX = speedX();
                if( ((!on_slippery_surface)&&(velX>0.0))||((on_slippery_surface)&&(_accelX>0.0)) )
                    animator.switchAnimation(MatrixAnimator::Run, _direction,
                                               (100-((velX*12)<85 ? velX*12 : 85)) );
                else if( ((!on_slippery_surface)&& (velX<0.0))||((on_slippery_surface)&&(_accelX<0.0)) )
                    animator.switchAnimation(MatrixAnimator::Run, _direction,
                                             (100-((-velX*12)<85 ? -velX*12 : 85)) );
                else
                    animator.switchAnimation(MatrixAnimator::Idle, _direction, 64);
            }
        }
    /**********************************Animation switcher**********************************/
}

void LVL_Player::kill(deathReason reason)
{
    doKill=true;
    kill_reason=reason;
}

void LVL_Player::harm(int _damage)
{
    doHarm=true;
    doHarm_damage=_damage;
}

void LVL_Player::bump(bool _up, double bounceSpeed, int timeToJump)
{
    if(_up)
    {
        bumpUp=true;
        bumpJumpTime = (timeToJump>0) ? timeToJump: physics_cur.jump_time_bounce ;
        bumpJumpVelocity = (bounceSpeed>0) ? bounceSpeed : physics_cur.velocity_jump_bounce;
    }
    else
    {
        bumpDown=true;
        bumpVelocity = fabs(bounceSpeed)/4.0;
    }
}

void LVL_Player::attack(LVL_Player::AttackDirection _dir)
{
    PGE_RectF attackZone;

    switch(_dir)
    {
        case Attack_Up:
            attackZone.setRect(posCenterX()-5, top()-17, 10, 5);
        break;
        case Attack_Down:
            attackZone.setRect(posCenterX()-5, bottom(), 10, 5);
        break;
        case Attack_Forward:
            if(_direction>=0)
                attackZone.setRect(right(), bottom()-32, 10, 10);
            else
                attackZone.setRect(left()-10, bottom()-32, 10, 10);
        break;
    }


    QVector<PGE_Phys_Object*> bodies;
    LvlSceneP::s->queryItems(attackZone, &bodies);
    int contacts = 0;

    QList<LVL_Block *> target_blocks;
    QList<LVL_Npc*> target_npcs;
    for(PGE_RenderList::iterator it=bodies.begin();it!=bodies.end(); it++ )
    {
        PGE_Phys_Object*visibleBody=*it;
        contacts++;
        if(visibleBody==this) continue;
        if(visibleBody==NULL)
            continue;
        switch(visibleBody->type)
        {
            case PGE_Phys_Object::LVLBlock:
                target_blocks.push_back(static_cast<LVL_Block*>(visibleBody));
                break;
            case PGE_Phys_Object::LVLNPC:
                target_npcs.push_back(static_cast<LVL_Npc*>(visibleBody));
                break;
            case PGE_Phys_Object::LVLPlayer:
                default:break;
        }
    }

    foreach(LVL_Block *x, target_blocks)
    {
        if(!x) continue;
        if(x->destroyed) continue;
        if(x->sizable && _dir==Attack_Forward)
            continue;
        x->hit();
        if(!x->destroyed)
        {
            LvlSceneP::s->launchStaticEffectC(69, x->posCenterX(), x->posCenterY(), 1, 0, 0, 0, 0);
            PGE_Audio::playSoundByRole(obj_sound_role::WeaponExplosion);
        }
        x->destroyed=true;
    }
    foreach(LVL_Npc *x, target_npcs)
    {
        if(!x) continue;
        if(x->isKilled()) continue;
        x->harm();
        LvlSceneP::s->launchStaticEffectC(75, attackZone.center().x(), attackZone.center().y(), 1, 0, 0, 0, 0);
        kill_npc(x, NPC_Kicked);
    }
}


//Enter player to level
void LVL_Player::WarpTo(float x, float y, int warpType, int warpDirection)
{
    warpFrameW = texture.w;
    warpFrameH = texture.h;

    switch(warpType)
    {
        case 2://door
        {
                setGravityScale(0);
                setSpeed(0, 0);
                EventQueueEntry<LVL_Player >event2;
                event2.makeCaller([this,x,y]()->void{
                                      isWarping=true; setPaused(true);
                                      warpPipeOffset=0.0;
                                      warpDirectO=0;
                                      teleport(x+16-_width_half,
                                                     y+32-_height);
                                      animator.unlock();
                                      animator.switchAnimation(MatrixAnimator::PipeUpDown, _direction, 115);
                                  }, 0);
                event_queue.events.push_back(event2);

                EventQueueEntry<LVL_Player >fadeOutBlack;
                fadeOutBlack.makeCaller([this]()->void{
                                      if(!camera->fader.isNull())
                                          camera->fader.setFade(10, 0.0, 0.08);
                                  }, 0);
                event_queue.events.push_back(fadeOutBlack);

                EventQueueEntry<LVL_Player >event3;
                event3.makeCaller([this]()->void{
                                      isWarping=false; setSpeed(0, 0); setPaused(false);
                                      last_environment=-1;//!< Forcing to refresh physical environment
                                  }, 200);
                event_queue.events.push_back(event3);
        }
        break;
    case 1://Pipe
        {
            // Exit direction: [1] down [3] up [4] left [2] right
            setGravityScale(0);
            setSpeed(0, 0);

            EventQueueEntry<LVL_Player >eventPipeEnter;
            eventPipeEnter.makeCaller([this,warpDirection]()->void{
                                    isWarping=true; setPaused(true);
                                    warpDirectO=warpDirection;
                                    warpPipeOffset=1.0;
                              }, 0);
            event_queue.events.push_back(eventPipeEnter);

            // Exit direction: [1] down [3] up [4] left [2] right
            switch(warpDirection)
            {
                case 2://right
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,x,y]()->void{
                                _direction=1;
                                animator.unlock();
                                animator.switchAnimation(MatrixAnimator::Run, _direction, 115);
                                teleport(x, y+32-_height);
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                    break;
                case 1://down
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,x,y]()->void{
                                animator.unlock();
                                animator.switchAnimation(MatrixAnimator::PipeUpDown, _direction, 115);
                                teleport(x+16-_width_half, y);
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                    break;
                case 4://left
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,x, y]()->void{
                                _direction=-1;
                                animator.unlock();
                                animator.switchAnimation(MatrixAnimator::Run, _direction, 115);
                                teleport(x+32-_width, y+32-_height);
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                    break;
                case 3://up
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,x,y]()->void{
                                animator.unlock();
                                animator.switchAnimation(MatrixAnimator::PipeUpDown, _direction, 115);
                                teleport(x+16-_width_half,
                                         y+32-_height);
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                    break;
                default:
                    break;
            }

            EventQueueEntry<LVL_Player >fadeOutBlack;
            fadeOutBlack.makeCaller([this]()->void{
                                  if(!camera->fader.isNull())
                                      camera->fader.setFade(10, 0.0, 0.08);
                              }, 0);
            event_queue.events.push_back(fadeOutBlack);

            EventQueueEntry<LVL_Player >wait200ms;
            wait200ms.makeTimer(250);
            event_queue.events.push_back(wait200ms);

            EventQueueEntry<LVL_Player >playSnd;
            playSnd.makeCaller([this]()->void{PGE_Audio::playSoundByRole(obj_sound_role::WarpPipe);
                                        },0);
            event_queue.events.push_back(playSnd);

            float pStep = 1.5f/((float)PGE_Window::TicksPerSecond);

            EventQueueEntry<LVL_Player >warpOut;
            warpOut.makeWaiterCond([this, pStep]()->bool{
                                      warpPipeOffset -= pStep;
                                      return warpPipeOffset<=0.0f;
                                  }, false, 0);
            event_queue.events.push_back(warpOut);

            EventQueueEntry<LVL_Player >endWarping;
            endWarping.makeCaller([this]()->void{
                                  isWarping=false; setSpeed(0, 0); setPaused(false);
                                  last_environment=-1;//!< Forcing to refresh physical environment
                              }, 0);
            event_queue.events.push_back(endWarping);
        }
        break;
    case 0://Instant
        teleport(x+16-_width_half,
                     y+32-_height);
        break;
    default:
        break;
    }
}


void LVL_Player::WarpTo(LevelDoor warp)
{
    warpFrameW = texture.w;
    warpFrameH = texture.h;

    switch(warp.type)
    {
        case 1:/*******Pipe!*******/
        {
            setSpeed(0, 0);
            setPaused(true);

            //Create events
            EventQueueEntry<LVL_Player >event1;
            event1.makeCaller([this]()->void{
                                setSpeed(0, 0); setPaused(true);
                                isWarping=true;
                                warpPipeOffset=0.0f;
                                setDuck(false);
                                PGE_Audio::playSoundByRole(obj_sound_role::WarpPipe);                                
                              }, 0);
            event_queue.events.push_back(event1);

            // Entrance direction: [3] down, [1] up, [2] left, [4] right
            switch(contactedWarp->data.idirect)
            {
                case 4://Right
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,warp]()->void{
                                  warpDirectO=4;
                                  _direction=1;
                                  animator.unlock();
                                  animator.switchAnimation(MatrixAnimator::Run, _direction, 115);
                                  setPos(warp.ix+32-_width, posY());
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                break;
                case 3: //Down
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,warp]()->void{
                                  warpDirectO=3;
                                  animator.unlock();
                                  animator.switchAnimation(MatrixAnimator::PipeUpDown, _direction, 115);
                                  setPos(posX(), warp.iy+32-_height);
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                break;
                case 2://Left
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,warp]()->void{
                                  warpDirectO=2;
                                  _direction=-1;
                                  animator.unlock();
                                  animator.switchAnimation(MatrixAnimator::Run, _direction, 115);
                                  setPos(warp.ix, posY());
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                break;
                case 1: //Up
                    {
                        EventQueueEntry<LVL_Player >eventX;
                        eventX.makeCaller([this,warp]()->void{
                                  warpDirectO=1;
                                  animator.unlock();
                                  animator.switchAnimation(MatrixAnimator::PipeUpDown, _direction, 115);
                                  setPos(posX(), warp.iy);
                                          }, 0);
                        event_queue.events.push_back(eventX);
                    }
                break;
            }

            float pStep = 1.5f/((float)PGE_Window::TicksPerSecond);
            EventQueueEntry<LVL_Player >warpIn;
            warpIn.makeWaiterCond([this, pStep]()->bool{
                                      warpPipeOffset += pStep;
                                      return warpPipeOffset>=1.0f;
                                  }, false, 0);
            event_queue.events.push_back(warpIn);

            EventQueueEntry<LVL_Player >wait200ms;
            wait200ms.makeTimer(250);
            event_queue.events.push_back(wait200ms);

            if( (warp.lvl_o) || (!warp.lname.isEmpty()) )
            {
                EventQueueEntry<LVL_Player >event2;
                event2.makeCaller([this, warp]()->void{
                          LvlSceneP::s->lastWarpID=warp.array_id;
                          exitFromLevel(warp.lname, warp.warpto,
                                        warp.world_x, warp.world_y);
                                  }, 200);
                event_queue.events.push_back(event2);
            }
            else
            {
                int sID = LvlSceneP::s->findNearestSection(warp.ox, warp.oy);
                if(camera->section->id != LvlSceneP::s->levelData()->sections[sID].id)
                {
                    EventQueueEntry<LVL_Player >event3;
                    event3.makeCaller([this]()->void{
                                          camera->fader.setFade(10, 1.0, 0.08);
                                      }, 0);
                    event_queue.events.push_back(event3);
                }

                EventQueueEntry<LVL_Player >whileOpacityFade;
                whileOpacityFade.makeWaiterCond([this]()->bool{
                                          return camera->fader.isFading();
                                      }, true, 100);
                event_queue.events.push_back(whileOpacityFade);

                WarpTo(warp.ox, warp.oy, warp.type, warp.odirect);
            }
        }
        break;
        case 2:/*******Door!*******/
        {
            setSpeed(0, 0);
            setPaused(true);
            //Create events
            EventQueueEntry<LVL_Player >event1;
            event1.makeCaller([this]()->void{
                                setSpeed(0, 0); setPaused(true); isWarping=true;
                                warpPipeOffset=0.0;
                                warpDirectO=0;
                                setDuck(false);
                                animator.unlock();
                                animator.switchAnimation(MatrixAnimator::PipeUpDownRear, _direction, 115);
                                PGE_Audio::playSoundByRole(obj_sound_role::WarpDoor);
                              }, 0);
            event_queue.events.push_back(event1);

            EventQueueEntry<LVL_Player >event2;
            event2.makeTimer(200);
            event_queue.events.push_back(event2);

            if( (warp.lvl_o) || (!warp.lname.isEmpty()) )
            {
                EventQueueEntry<LVL_Player >event2;
                event2.makeCaller([this, warp]()->void{
                          LvlSceneP::s->lastWarpID=warp.array_id;
                          exitFromLevel(warp.lname, warp.warpto,
                                        warp.world_x, warp.world_y);
                                  }, 200);
                event_queue.events.push_back(event2);

            }
            else
            {
                int sID = LvlSceneP::s->findNearestSection(warp.ox, warp.oy);
                if(camera->section->id != LvlSceneP::s->levelData()->sections[sID].id)
                {
                    EventQueueEntry<LVL_Player >event3;
                    event3.makeCaller([this]()->void{
                                          camera->fader.setFade(10, 1.0, 0.08);
                                      }, 0);
                    event_queue.events.push_back(event3);
                }

                EventQueueEntry<LVL_Player >event4;
                event4.makeWaiterCond([this]()->bool{
                                          return camera->fader.isFading();
                                      }, true, 100);
                event_queue.events.push_back(event4);

                WarpTo(warp.ox, warp.oy, warp.type, warp.odirect);
            }
        break;
        }
    }
    event_queue.processEvents(0);
}


void LVL_Player::teleport(float x, float y)
{
    if(!LvlSceneP::s) return;

    this->setPos(x, y);

    int sID = LvlSceneP::s->findNearestSection(x, y);
    LVL_Section* t_sct = LvlSceneP::s->getSection(sID);

    if(t_sct)
    {
        camera->changeSection(t_sct);
        setParentSection(t_sct);
    }
    else
    {
        camera->resetLimits();
    }
}

void LVL_Player::exitFromLevel(QString levelFile, int targetWarp, long wX, long wY)
{
    isAlive = false;
    if(!levelFile.isEmpty())
    {
        LvlSceneP::s->warpToLevelFile =
                LvlSceneP::s->levelData()->path+"/"+levelFile;
        LvlSceneP::s->warpToArrayID = targetWarp;
    }
    else
    {
        if((wX != -1)&&( wY != -1))
        {
            LvlSceneP::s->warpToWorld=true;
            LvlSceneP::s->warpToWorldXY.setX(wX);
            LvlSceneP::s->warpToWorldXY.setY(wY);
        }
    }
    LvlSceneP::s->setExiting(2000, LvlExit::EXIT_Warp);
}



void LVL_Player::kill_npc(LVL_Npc *target, LVL_Player::kill_npc_reasons reason)
{
    if(!target) return;
    if(!target->isKilled()) return;

    switch(reason)
    {
        case NPC_Unknown:
            break;
        case NPC_Stomped:
            PGE_Audio::playSoundByRole(obj_sound_role::PlayerStomp); break;
        case NPC_Kicked:
            PGE_Audio::playSoundByRole(obj_sound_role::PlayerKick); break;
        case NPC_Taked_Coin:
            PGE_Audio::playSoundByRole(obj_sound_role::BonusCoin); break;
        case NPC_Taked_Powerup:
            break;
    }

    if(target->setup->exit_is)
    {
        long snd=target->setup->exit_snd;
        if(snd<=0)
        {
            switch(target->setup->exit_code)
            {
                case  1: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit01); break;
                case  2: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit02); break;
                case  3: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit03); break;
                case  4: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit04); break;
                case  5: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit05); break;
                case  6: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit06); break;
                case  7: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit07); break;
                case  8: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit08); break;
                case  9: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit09); break;
                case 10: snd=ConfigManager::getSoundByRole(obj_sound_role::LevelExit10); break;
                default: break;
            }
        }
        if(snd>0)
        {
            PGE_MusPlayer::MUS_stopMusic();
            PGE_Audio::playSound(snd);
        }
        /***********************Reset and unplug controllers************************/
        LvlSceneP::s->player1Controller->resetControls();
        LvlSceneP::s->player1Controller->sendControls();
        LvlSceneP::s->player1Controller->removeFromControl(this);
        LvlSceneP::s->player2Controller->resetControls();
        LvlSceneP::s->player2Controller->sendControls();
        LvlSceneP::s->player2Controller->removeFromControl(this);
        /***********************Reset and unplug controllers*end********************/
        if(target->setup->exit_walk_direction<0)
            keys.left=true;
        else
        if(target->setup->exit_walk_direction>0)
            keys.right=true;
        isExiting=true;
        LvlSceneP::s->setExiting(target->setup->exit_delay, target->setup->exit_code);
    }
}

bool LVL_Player::isRunning()
{
    return _isRunning;
}


int LVL_Player::direction()
{
    return _direction;
}


void LVL_Player::render(double camX, double camY)
{
    if(!isAlive) return;
    if(!_isInited) return;
    if(_no_render) return;

    PGE_RectF tPos = animator.curFrame();
    PGE_PointF Ofs = animator.curOffset();

    PGE_RectF player;
    player.setRect(round(posX()-camX)-Ofs.x(),
                   round(posY()-Ofs.y())-camY,
                            frameW,
                            frameH
                         );

    if(isWarping)
    {
        if(warpPipeOffset>=1.0)
            return;
        //     Exit direction: [1] down  [3] up  [4] left  [2] right
        // Entrance direction: [3] down, [1] up, [2] left, [4] right
        switch(warpDirectO)
        {
            case 2://Left entrance, right Exit
                {
                    float wOfs = Ofs.x()/warpFrameW;//Relative X offset
                    float wOfsF = _width/warpFrameW; //Relative hitbox width
                    tPos.setLeft(tPos.left()+wOfs+(warpPipeOffset*wOfsF));
                    player.setLeft( player.left()+Ofs.x() );
                    player.setRight( player.right()-(warpPipeOffset*_width) );
                }
                break;
            case 1://Up entrance, down exit
                {
                    float hOfs = Ofs.y()/warpFrameH;//Relative Y offset
                    float hOfsF = _height/warpFrameH; //Relative hitbox Height
                    tPos.setTop(tPos.top()+hOfs+(warpPipeOffset*hOfsF));
                    player.setTop( player.top()+Ofs.y() );
                    player.setBottom( player.bottom()-(warpPipeOffset*_height) );
                }
                break;
            case 4://right emtramce. left exit
                {
                    float wOfs =  Ofs.x()/warpFrameW;               //Relative X offset
                    float fWw =   animator.sizeOfFrame().w();   //Relative width of frame
                    float wOfHB = _width/warpFrameW;                 //Relative width of hitbox
                    float wWAbs = warpFrameW*fWw;                   //Absolute width of frame
                    tPos.setRight(tPos.right()-(fWw-wOfHB-wOfs)-(warpPipeOffset*wOfHB));
                    player.setLeft( player.left()+(warpPipeOffset*_width) );
                    player.setRight( player.right()-(wWAbs-Ofs.x()-_width) );
                }
                break;
            case 3://down entrance, up exit
                {
                    float hOfs =  Ofs.y()/warpFrameH;               //Relative Y offset
                    float fHh =   animator.sizeOfFrame().h();  //Relative height of frame
                    float hOfHB = _height/warpFrameH;                //Relative height of hitbox
                    float hHAbs = warpFrameH*fHh;                   //Absolute height of frame
                    tPos.setBottom(tPos.bottom()-(fHh-hOfHB-hOfs)-(warpPipeOffset*hOfHB));
                    player.setTop( player.top()+(warpPipeOffset*_height) );
                    player.setBottom( player.bottom()-(hHAbs-Ofs.y()-_height) );
                }
                break;
            default:
                break;
        }
    }


    GlRenderer::renderTexture(&texture,
                              player.x(),
                              player.y(),
                              player.width(),
                              player.height(),
                              tPos.top(),
                              tPos.bottom(),
                              tPos.left(),
                              tPos.right());

    if(PGE_Window::showDebugInfo)
    {
        //FontManager::printText(QString("%1-%2").arg(characterID).arg(stateID), round(posX()-camX), round(posY()-camY));
        FontManager::printText(QString(" %1 \n%2%3%4\n %5 ")
                               .arg(collided_top.size())
                               .arg(collided_left.size())
                               .arg(collided_center.size())
                               .arg(collided_right.size())
                               .arg(collided_bottom.size())
                               , round(posX()-camX), round(posY()-camY), 3);
    }

}

bool LVL_Player::locked()
{
    return isLocked;
}

void LVL_Player::setLocked(bool lock)
{
    isLocked=lock;
    setPaused(lock);
}

bool LVL_Player::isInited()
{
    return _isInited;
}

