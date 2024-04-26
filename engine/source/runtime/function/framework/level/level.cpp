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

#include "runtime/core/math/math.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_system.h"

#include <algorithm>
#include <limits>
#include <queue>

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

    void Level::generatePath(bool*             mazeDoors,
                             const int&        rows,
                             const int&        cols,
                             MazePositionIndex startPos,
                             MazePositionIndex endPos)
    {
        // Helper functions
        auto getDir = [mazeDoors, rows, cols](const MazePositionIndex& index, int dir) -> bool {
            return mazeDoors[index.x * cols * 4 + index.y * 4 + dir];
        };
        auto checkValid = [&rows, &cols](const MazePositionIndex& index) -> bool {
            return index.x >= 0 && index.x < rows && index.y >= 0 && index.y < cols;
        };
        // Because it is a grid map, we use Manhattan distance.
        auto ManhattanDis = [](const MazePositionIndex& index_A, const MazePositionIndex& index_B) -> int {
            return Math::abs(index_A.x - index_B.x) + Math::abs(index_A.y - index_B.y);
        };
        auto GeometricDis = [](const MazePositionIndex& index_A, const MazePositionIndex& index_B) -> float {
            return Math::sqrt(Math::sqr(index_A.x - index_B.x) + Math::sqr(index_A.y - index_B.y));
        };
        auto CulculateCost = [startPos, endPos, ManhattanDis, GeometricDis](MazePositionIndex curIndex) -> float {
            return GeometricDis(startPos, curIndex) + ManhattanDis(endPos, curIndex);
        };

        // Initializes the data structure of the open close set in the maze
        std::priority_queue<MazeNode, std::vector<MazeNode>, std::greater<MazeNode>> open;
        std::unordered_set<MazePositionIndex>                                        close;
        /*std::unordered_map<MazePositionIndex,MazePositionIndex> m_path*/

        std::vector<std::vector<bool>> openLUT(rows, std::vector<bool>(cols, true));
        auto                           startPoint = MazeNode(startPos, 0, 0);
        open.push(startPoint);
        close.clear();
        openLUT[startPos.x][startPos.y] =
            false; // open priority queue look up table for check whether this element is in the queue
        // // true:can break    false: cant break
        MazePositionIndex offset[4] = {
            {-1, 0}, // 0:Up
            {0, +1}, // 1:Right
            {+1, 0}, // 2:Down
            {0, -1}  // 3:Left
        };           // maze node's dir

        while (!open.empty())
        {
            auto mazeNodeTemp = open.top();
            open.pop();
            close.insert(mazeNodeTemp.index);
            openLUT[mazeNodeTemp.index.x][mazeNodeTemp.index.y] = true;
            if (mazeNodeTemp.index == endPos)
            {
                break;
            }
            for (auto i = 0; i < 4; i++)
            {
                MazePositionIndex MPI_temp(mazeNodeTemp.index + offset[i]);
                if (getDir(mazeNodeTemp.index, i) && checkValid(MPI_temp) && close.find(MPI_temp) == close.end())
                {
                    MazeNode temp(MPI_temp, mazeNodeTemp.G + 1, ManhattanDis(endPos, MPI_temp));
                    if (openLUT[MPI_temp.x][MPI_temp.y])
                    {
                        openLUT[MPI_temp.x][MPI_temp.y] = false;
                        m_path[temp.index]              = mazeNodeTemp;
                        open.push(temp);
                    }
                    else
                    {
                        if (m_path[temp.index].G > temp.G)
                        {

                            m_path[temp.index].G     = temp.G;
                            m_path[temp.index].cost  = m_path[temp.index].G + m_path[temp.index].H;
                            m_path[temp.index].index = temp.index;
                        }
                    }
                }
            }
        }

        std::vector<MazePositionIndex> path;
        auto                           iter = endPos;
        while (iter != startPos)
        {
            path.push_back(iter);
            iter = m_path[iter].index;
        }
        path.push_back(startPos);
        std::reverse(path.begin(), path.end());
        LOG_INFO("Path generate success!");
        auto i = 1 + 1;
    }

    void Level::generateMaze()
    {
        // 1:Iterate through and delete all existing entities in the scene
        auto iter = m_gobjects.begin();
        while (iter != m_gobjects.end())
        {
            // Delete render entities
            RenderSwapContext& swap_context = g_runtime_global_context.m_render_system->getSwapContext();
            swap_context.getLogicSwapData().addDeleteGameObject(GameObjectDesc {iter->first, {}});
            // Delete in the level class
            deleteGObjectByID(iter->first);
            iter = m_gobjects.begin();
        }

        // 2:Generate startPos and endPos
        ObjectInstanceRes Player;
        Player.m_name       = "Player";
        Player.m_definition = "asset/objects/character/player/player.object.json";
        createObject(Player);
        // TODO: endPos

        // 3:rows and cols
        const int cols = 20;
        const int rows = 15;

        // 4:Generate the ground
        const float ground_W        = 87.1536;
        const float ground_L        = 49.7335;
        float       width_of_maze   = rows * 10;
        float       length_of_maze  = cols * 10;
        int         tiles_of_width  = static_cast<int>(width_of_maze / ground_W) + 1;
        int         tiles_of_length = static_cast<int>(length_of_maze / ground_L) + 1;
        for (auto i = 0; i < tiles_of_width * tiles_of_length; i++)
        {
            ObjectInstanceRes Ground;
            Ground.m_name       = "Ground_" + std::to_string(i);
            Ground.m_definition = "asset/objects/environment/floor/floor.object.json";
            createObject(Ground);
        }

        // 5:Generate the struct of Maze
        int  mazeTypes[rows][cols];
        bool mazeDoors[rows][cols][4]; // Maze node Dir -> 0:Up      1:Right     2:Down      3:Left   true:can break
                                       // false: cant break
        // Init maze
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                // 初始化迷宫房间的编号
                mazeTypes[i][j] = i * cols + j;
                for (int z = 0; z < 4; z++)
                {
                    // 初始化房间墙壁的状态
                    mazeDoors[i][j][z] = false;
                }
            }
        }

        // Build maze
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                std::vector<int> candidateDoorsDir;
                if (i > 0 && mazeTypes[i - 1][j] != mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(0);
                }
                // check right
                if (j < cols - 1 && mazeTypes[i][j + 1] != mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(1);
                }
                // check down
                if (i < rows - 1 && mazeTypes[i + 1][j] != mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(2);
                }
                // check right
                if (j > 0 && mazeTypes[i][j - 1] != mazeTypes[i][j])
                {
                    candidateDoorsDir.push_back(3);
                }
                if (candidateDoorsDir.size() == 0)
                {
                    break;
                }
                // Randomly break wall
                int openDoorDir = candidateDoorsDir[std::rand() % candidateDoorsDir.size()];
                int newRoomID;
                mazeDoors[i][j][openDoorDir] = true;
                switch (openDoorDir) // 0:Up      1:Right     2:Down      3:Left
                {
                    case 0:
                        mazeDoors[i - 1][j][2] = true;
                        newRoomID              = mazeTypes[i - 1][j];
                        break;
                    case 1:
                        mazeDoors[i][j + 1][3] = true;
                        newRoomID              = mazeTypes[i][j + 1];
                        break;
                    case 2:
                        mazeDoors[i + 1][j][0] = true;
                        newRoomID              = mazeTypes[i + 1][j];
                        break;
                    case 3:
                        mazeDoors[i][j - 1][1] = true;
                        newRoomID              = mazeTypes[i][j - 1];
                        break;

                    default:
                        break;
                }
                int oldDoorID = mazeTypes[i][j];
                for (int ii = 0; ii < rows; ii++)
                {
                    for (int jj = 0; jj < cols; jj++)
                    {
                        if (mazeTypes[ii][jj] == oldDoorID)
                        {
                            mazeTypes[ii][jj] = newRoomID;
                        }
                    }
                }
            }
        }

        // 6:Generate the walls
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                // Up
                if (!mazeDoors[i][j][0])
                {
                    int               wallNum = i * (2 * cols + 1) + j;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                // Left
                if (!mazeDoors[i][j][3])
                {
                    int               wallNum = i * (2 * cols + 1) + j + cols;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                // Right
                if (j == cols - 1)
                {
                    int               wallNum = i * (2 * cols + 1) + j + cols + 1;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
                // Down
                if (i == rows - 1)
                {
                    int               wallNum = i * (2 * cols + 1) + j + 2 * cols + 1;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    createObject(Wall);
                }
            }
        }

        // 7:Place the object in the maze
        for (const auto& object_pair : m_gobjects)
        {
            std::shared_ptr<GObject> object = object_pair.second;
            if (object == nullptr)
            {
                continue;
            }
            if ("Player" == object->getName())
            {
                m_current_active_character              = std::make_shared<Character>(object);
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                Vector3             startPosition;
                startPosition.x = -10 - 10 * (rows - 1) / 2 + 0 * 10 + 5;
                startPosition.y = -10 * (cols - 1) / 2 + 0 * 10;
                startPosition.z = 0;
                transform_component->setPosition(startPosition);
                continue;
            }
            if ("Wall_" == (object->getName().substr(0, 5)))
            {
                int                 i                   = std::stoi(object->getName().substr(5));
                int                 rowNum              = int(i / (2 * cols + 1));
                int                 colNum              = i % (2 * cols + 1);
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                Vector3             new_translation;
                Quaternion          new_rotation;
                if (colNum < cols)
                {
                    new_translation.x = -10 - 10 * (rows - 1) / 2 + rowNum * 10;
                    new_translation.y = -10 * (cols - 1) / 2 + colNum * 10;
                    new_translation.z = 0;
                }
                else
                {
                    colNum -= cols;
                    new_translation.x = -5 - 10 * (rows - 1) / 2 + rowNum * 10;
                    new_translation.y = -5 - 10 * (cols - 1) / 2 + colNum * 10;
                    Vector3 axis(0, 0, 1);
                    Degree  d(90.0);
                    Radian  angle(d);
                    new_rotation.fromAngleAxis(angle, axis);
                }
                transform_component->setPosition(new_translation);
                transform_component->setRotation(new_rotation);
            }

            if ("Ground_" == (object->getName().substr(0, 7)))
            {
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                int                 i                   = std::stoi(object->getName().substr(7));
                int                 wi                  = i % tiles_of_width;
                int                 li                  = i / tiles_of_width;

                Vector2 corner = {-10 - 10 * (rows - 1) / 2, -10 * (cols - 1) / 2 - 5};
                float   set_X  = corner.x + ground_W / 2.0;
                float   set_Y  = corner.y + ground_L / 2.0;
                set_X += ground_W * wi - (tiles_of_width * ground_W - width_of_maze) / 2.0;
                set_Y += ground_L * li - (tiles_of_length * ground_L - length_of_maze) / 2.0;

                Vector3 fixPosition(set_X, set_Y, 0);
                transform_component->setPosition(fixPosition);
                continue;
            }
        }
        // 8:generate the path from startPos to endPos
        generatePath(&mazeDoors[0][0][0], rows, cols, {0, 0}, {rows - 1, cols - 1});
    }

} // namespace Piccolo
