#include "runtime/function/framework/level/level.h"

#include "runtime/core/base/macro.h"

#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/res_type/common/level.h"

#include "runtime/engine.h"
#include "runtime/function/character/character.h"
#include "runtime/function/framework/object/object.h"
#include "runtime/function/particle/particle_manager.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/physics/physics_scene.h"

#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_system.h"

#include <limits>

namespace Piccolo
{
    void Level::clear()
    {
        m_current_active_character.reset();
        m_gobjects.clear();

        ASSERT(g_runtime_global_context.m_physics_manager);
        g_runtime_global_context.m_physics_manager->deletePhysicsScene(m_physics_scene);
    }

    GObjectID Level::createObject(const ObjectInstanceRes& object_instance_res)
    {
        GObjectID object_id = ObjectIDAllocator::alloc();
        ASSERT(object_id != k_invalid_gobject_id);

        std::shared_ptr<GObject> gobject;
        try
        {
            gobject = std::make_shared<GObject>(object_id);
        }
        catch (const std::bad_alloc&)
        {
            LOG_FATAL("cannot allocate memory for new gobject");
        }

        bool is_loaded = gobject->load(object_instance_res);
        if (is_loaded)
        {
            m_gobjects.emplace(object_id, gobject);
        }
        else
        {
            LOG_ERROR("loading object " + object_instance_res.m_name + " failed");
            return k_invalid_gobject_id;
        }
        return object_id;
    }

    bool Level::load(const std::string& level_res_url)
    {
        LOG_INFO("loading level: {}", level_res_url);

        m_level_res_url = level_res_url;

        LevelRes   level_res;
        const bool is_load_success = g_runtime_global_context.m_asset_manager->loadAsset(level_res_url, level_res);
        if (is_load_success == false)
        {
            return false;
        }

        ASSERT(g_runtime_global_context.m_physics_manager);
        m_physics_scene = g_runtime_global_context.m_physics_manager->createPhysicsScene(level_res.m_gravity);
        ParticleEmitterIDAllocator::reset();

        for (const ObjectInstanceRes& object_instance_res : level_res.m_objects)
        {
            createObject(object_instance_res);
        }

        // create active character
        for (const auto& object_pair : m_gobjects)
        {
            std::shared_ptr<GObject> object = object_pair.second;
            if (object == nullptr)
                continue;

            if (level_res.m_character_name == object->getName())
            {
                m_current_active_character = std::make_shared<Character>(object);
                break;
            }
        }

        m_is_loaded = true;

        LOG_INFO("level load succeed");

        return true;
    }

    void Level::unload()
    {
        clear();
        LOG_INFO("unload level: {}", m_level_res_url);
    }

    bool Level::save()
    {
        LOG_INFO("saving level: {}", m_level_res_url);
        LevelRes output_level_res;

        const size_t                    object_cout    = m_gobjects.size();
        std::vector<ObjectInstanceRes>& output_objects = output_level_res.m_objects;
        output_objects.resize(object_cout);

        size_t object_index = 0;
        for (const auto& id_object_pair : m_gobjects)
        {
            if (id_object_pair.second)
            {
                id_object_pair.second->save(output_objects[object_index]);
                ++object_index;
            }
        }

        const bool is_save_success =
            g_runtime_global_context.m_asset_manager->saveAsset(output_level_res, m_level_res_url);

        if (is_save_success == false)
        {
            LOG_ERROR("failed to save {}", m_level_res_url);
        }
        else
        {
            LOG_INFO("level save succeed");
        }

        return is_save_success;
    }

    void Level::tick(float delta_time)
    {
        if (!m_is_loaded)
        {
            return;
        }

        for (const auto& id_object_pair : m_gobjects)
        {
            assert(id_object_pair.second);
            if (id_object_pair.second)
            {
                id_object_pair.second->tick(delta_time);
            }
        }
        if (m_current_active_character && g_is_editor_mode == false)
        {
            m_current_active_character->tick(delta_time);
        }

        std::shared_ptr<PhysicsScene> physics_scene = m_physics_scene.lock();
        if (physics_scene)
        {
            physics_scene->tick(delta_time);
        }
    }

    std::weak_ptr<GObject> Level::getGObjectByID(GObjectID go_id) const
    {
        auto iter = m_gobjects.find(go_id);
        if (iter != m_gobjects.end())
        {
            return iter->second;
        }

        return std::weak_ptr<GObject>();
    }

    void Level::deleteGObjectByID(GObjectID go_id)
    {
        auto iter = m_gobjects.find(go_id);
        if (iter != m_gobjects.end())
        {
            std::shared_ptr<GObject> object = iter->second;
            if (object)
            {
                if (m_current_active_character && m_current_active_character->getObjectID() == object->getID())
                {
                    m_current_active_character->setObject(nullptr);
                }
            }
        }

        m_gobjects.erase(go_id);
    }

    void Level::generateMaze()
    {
        //1:遍历并且删除场景中所有已有的实体
        auto iter = m_gobjects.begin();
        while(iter != m_gobjects.end())
        {
            //在渲染中删除实体
            RenderSwapContext& swap_context = g_runtime_global_context.m_render_system->getSwapContext();
            swap_context.getLogicSwapData().addDeleteGameObject(GameObjectDesc{iter->first,{}});

            //在关卡类中将其删除
            deleteGObjectByID(iter->first);
            iter = m_gobjects.begin();
        }
        //2:生成地面

        ObjectInstanceRes Ground;
        Ground.m_name = "Ground";
        Ground.m_definition = "asset/objects/environment/floor/floor.object.json";
        createObject(Ground);

        //3:生成角色
        ObjectInstanceRes Player;
        Player.m_name = "Player";
        Player.m_definition = "asset/objects/character/player/player.object.json";
        createObject(Player);

        //4:给定迷宫的列数与行数
        const int cols = 5;
        const int rows = 8;

        //5:生成迷宫的数据结构

        int mazeTypes[rows][cols];
        bool mazeDoors[rows][cols][4];//代表每个迷宫节点的方向
        //0:Up      1:Right     2:Down      3:Left
        //true:can break    false: cant break
        
        //初始化迷宫
        for(int i = 0;i <rows;i++)
        {
            for(int j = 0;j<cols;j++)
            {
                //初始化迷宫房间的编号
                mazeTypes[i][j] = i*cols+j;
                for(int z=0;z<4;z++)
                {
                    //初始化房间墙壁的状态
                    mazeDoors[i][j][z] = false;
                }
            }
        }

        // auto checkIndex = [&](int i, int j) -> bool
        // {
        //     return i >= 0 && i < rows && j >= 0 && j < cols;
        // };

        

        //开始构建迷宫
        for(int i = 0;i <rows;i++)
        {
            for(int j = 0;j<cols;j++)
            {
                std::vector<int> candidateDoorsDir;
                //检查上方
                // if(checkIndex(i-1,j)&&mazeTypes[i-1][j]!=mazeTypes[i][j])
                // {
                //     candidateDoorsDir.push_back(0);
                // }
                // //检查右边
                // if(checkIndex(i,j+1)&&mazeTypes[i][j+1]!=mazeTypes[i][j])
                // {
                //     candidateDoorsDir.push_back(1);
                // }
                // //检查下面
                // if(checkIndex(i+1,j)&&mazeTypes[i+1][j]!=mazeTypes[i][j])
                // {
                //     candidateDoorsDir.push_back(2);
                // }
                // //检查左边
                // if(checkIndex(i,j-1)&&mazeTypes[i][j-1]!=mazeTypes[i][j])
                // {
                //     candidateDoorsDir.push_back(3);
                // }
                if(i>0&&mazeTypes[i-1][j]!=mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(0);
                }
                //检查右边
                if(j<cols-1&&mazeTypes[i][j+1]!=mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(1);
                }
                //检查下面
                if(i<rows-1&&mazeTypes[i+1][j]!=mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(2);
                }
                //检查左边
                if(j>0&&mazeTypes[i][j-1]!=mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(3);
                }
                if(candidateDoorsDir.size()==0)
                {
                    break;
                }

                //随机在能够拆除的墙上选择一个进行拆除
                int openDoorDir = candidateDoorsDir[std::rand()%candidateDoorsDir.size()];
                int newRoomID;
                mazeDoors[i][j][openDoorDir] = true;
                switch (openDoorDir)        //0:Up      1:Right     2:Down      3:Left
                {
                case 0:
                    mazeDoors[i-1][j][2] = true;
                    newRoomID = mazeTypes[i-1][j];
                    break;
                case 1:
                    mazeDoors[i][j+1][3] = true;
                    newRoomID = mazeTypes[i][j+1];
                    break;
                case 2:
                    mazeDoors[i+1][j][0] = true;
                    newRoomID = mazeTypes[i+1][j];
                    break;
                case 3:
                    mazeDoors[i][j-1][1] = true;
                    newRoomID = mazeTypes[i][j-1];
                    break;
                
                default:
                    break;
                }

                int oldDoorID = mazeTypes[i][j];
                for(int ii = 0;ii<rows;ii++)
                {
                    for(int jj =0;jj<cols;jj++)
                    {
                        if(mazeTypes[ii][jj]==oldDoorID)
                        {
                            mazeTypes[ii][jj]=newRoomID;
                        }
                    }
                }
                
            }
        }

        //6:根据迷宫的数据结构生成墙壁
        for(int i = 0;i<rows;i++)
        {
            for(int j = 0;j<cols;j++)
            {
                //上面的墙壁
                if(!mazeDoors[i][j][0])
                {
                    int wallNum = i*(2*cols +1)+j;
                    ObjectInstanceRes Wall;
                    Wall.m_name = "Wall_"+std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                //左边的墙壁
                if(!mazeDoors[i][j][3])
                {
                    int wallNum = i*(2*cols +1)+j+cols;
                    ObjectInstanceRes Wall;
                    Wall.m_name = "Wall_"+std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                //最右边的墙壁
                if(j==cols-1)
                {
                    int wallNum = i*(2*cols +1)+j+cols+1;
                    ObjectInstanceRes Wall;
                    Wall.m_name = "Wall_"+std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                //最下面的墙壁
                if(i==rows-1)
                {
                    int wallNum = i*(2*cols +1)+j+2*cols+1;
                    ObjectInstanceRes Wall;
                    Wall.m_name = "Wall_"+std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
            }
        }

        for(const auto& object_pair :m_gobjects)
        {
            std::shared_ptr<GObject> object = object_pair.second;
            if(object == nullptr)
            {
                continue;
            }
            std::cout<<object->getName();

            if("Player" == object->getName())
            {
                m_current_active_character = std::make_shared<Character>(object);
                continue;
            }

            if("Ground" == object->getName())
            {
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                Vector3 fixPosition(-5.0,0,0);
                transform_component->setPosition(fixPosition);
                continue;
            }

            for(int i = 0;i<rows*(2*cols +1)+cols;i++)
            {
                if("Wall_"+std::to_string(i)==(object->getName()))
                {
                    int rowNum = int(i/(2*cols+1));
                    int colNum = i%(2*cols+1);
                    TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                    Vector3 new_translation;
                    Quaternion new_rotation;
                    if(colNum <cols)
                    {
                        new_translation.x = -10-10*(rows-1)/2+rowNum*10;
                        new_translation.y = -10*(cols-1)/2+colNum*10;
                        new_translation.z = 0;
                    }
                    else
                    {
                        colNum-=cols;
                        new_translation.x = -5-10*(rows-1)/2+rowNum*10;
                        new_translation.y = -5-10*(cols-1)/2+colNum*10;
                        Vector3 axis(0,0,1);
                        Degree d(90.0);
                        Radian angle(d);
                        new_rotation.fromAngleAxis(angle,axis);
                    }
                    transform_component->setPosition(new_translation);
                    transform_component->setRotation(new_rotation);
                }
            }
        }

    }

} // namespace Piccolo
