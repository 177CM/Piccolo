#include "runtime/function/framework/level/maze_manager.h"
#include "runtime/core/math/vector2.h"
#include "runtime/core/math/vector3.h"
#include "runtime/function/character/character.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/level/level.h"
#include "runtime/function/framework/object/object.h"
#include "runtime/function/render/render_swap_context.h"
#include "runtime/function/render/render_system.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <time.h>
#include <unordered_map>
#include <unordered_set>

namespace Piccolo
{

    void MazeManager::clearCurrentLevel(std::shared_ptr<Level> level)
    {
        auto iter = level->m_gobjects.begin();
        while (iter != level->m_gobjects.end())
        {
            // Delete render entities
            RenderSwapContext& swap_context = g_runtime_global_context.m_render_system->getSwapContext();
            swap_context.getLogicSwapData().addDeleteGameObject(GameObjectDesc {iter->first, {}});
            // Delete in the level class
            level->deleteGObjectByID(iter->first);
            iter = level->m_gobjects.begin();
        }
    }

    void MazeManager::generateMaze(std::shared_ptr<Level> level)
    {
        if (m_col <= 0 || m_row <= 0)
        {
            LOG_FATAL("The size of Maze has to be set first!");
            return;
        }
        // set random seed
        std::srand(std::time(nullptr));
        // 1:Iterate through and delete all existing entities in the scene
        clearCurrentLevel(level);

        // 2:Generate startPos and endPos
        ObjectInstanceRes Player;
        Player.m_name       = "Player";
        Player.m_definition = "asset/objects/character/player/player.object.json";
        level->createObject(Player);
        // TODO: endPos

        // 3:rows and cols
        // const int cols = 20;
        // const int rows = 15;

        // 4:Generate the ground
        const float ground_W        = 87.1536;
        const float ground_L        = 49.7335;
        float       width_of_maze   = m_row * 10;
        float       length_of_maze  = m_col * 10;
        int         tiles_of_width  = static_cast<int>(width_of_maze / ground_W) + 1;
        int         tiles_of_length = static_cast<int>(length_of_maze / ground_L) + 1;
        for (auto i = 0; i < tiles_of_width * tiles_of_length; i++)
        {
            ObjectInstanceRes Ground;
            Ground.m_name       = "Ground_" + std::to_string(i);
            Ground.m_definition = "asset/objects/environment/floor/floor.object.json";
            level->createObject(Ground);
        }

        // 5:Generate the struct of Maze
        std::vector<std::vector<int>>                           mazeIndex2Type(m_row, std::vector<int>(m_col));
        std::unordered_map<int, std::vector<MazePositionIndex>> mazeType2Indexs;
        std::vector<std::vector<std::vector<bool>>>             mazeDoors(
            m_row, std::vector<std::vector<bool>>(m_col, std::vector<bool>(4, false)));

        // Maze node Dir -> 0:Up      1:Right     2:Down      3:Left   true:can break  false: cant break
        // Init maze
        for (int i = 0; i < m_row; i++)
        {
            for (int j = 0; j < m_col; j++)
            {
                // init type of the maze node
                mazeIndex2Type[i][j]           = i * m_col + j;
                mazeType2Indexs[i * m_col + j] = {{i, j}};
            }
        }

        // Build maze
        for (int i = 0; i < m_row; i++)
        {
            for (int j = 0; j < m_col; j++)
            {
                std::vector<int> candidateDoorsDir;
                if (i > 0 && mazeIndex2Type[i - 1][j] != mazeIndex2Type[i][j])
                {
                    candidateDoorsDir.push_back(0);
                }
                // check right
                if (j < m_col - 1 && mazeIndex2Type[i][j + 1] != mazeIndex2Type[i][j])
                {
                    candidateDoorsDir.push_back(1);
                }
                // check down
                if (i < m_row - 1 && mazeIndex2Type[i + 1][j] != mazeIndex2Type[i][j])
                {
                    candidateDoorsDir.push_back(2);
                }
                // check right
                if (j > 0 && mazeIndex2Type[i][j - 1] != mazeIndex2Type[i][j])
                {
                    candidateDoorsDir.push_back(3);
                }
                if (candidateDoorsDir.size() == 0)
                {
                    break;
                }
                // Randomly break wall
                // TODO: add a real random by time!
                int openDoorDir = candidateDoorsDir[std::rand() % candidateDoorsDir.size()];
                int newRoomID;
                mazeDoors[i][j][openDoorDir] = true;
                switch (openDoorDir) // 0:Up      1:Right     2:Down      3:Left
                {
                    case 0:
                        mazeDoors[i - 1][j][2] = true;
                        newRoomID              = mazeIndex2Type[i - 1][j];
                        break;
                    case 1:
                        mazeDoors[i][j + 1][3] = true;
                        newRoomID              = mazeIndex2Type[i][j + 1];
                        break;
                    case 2:
                        mazeDoors[i + 1][j][0] = true;
                        newRoomID              = mazeIndex2Type[i + 1][j];
                        break;
                    case 3:
                        mazeDoors[i][j - 1][1] = true;
                        newRoomID              = mazeIndex2Type[i][j - 1];
                        break;

                    default:
                        break;
                }
                int oldDoorID = mazeIndex2Type[i][j];
                for (const auto pos : mazeType2Indexs[oldDoorID])
                {
                    mazeIndex2Type[pos.x][pos.y] = newRoomID;
                }
                mazeType2Indexs[newRoomID].insert(mazeType2Indexs[newRoomID].end(),
                                                  mazeType2Indexs[oldDoorID].begin(),
                                                  mazeType2Indexs[oldDoorID].end());
                mazeType2Indexs.erase(oldDoorID);
            }
        }

        // generate the path from startPos to endPos
        generatePath(mazeDoors, {0, 0}, {m_row - 1, m_col - 1});

        for (auto i = 0; i < m_path.size(); i++)
        {
            ObjectInstanceRes Hint;
            Hint.m_name       = "Hint_" + std::to_string(i);
            Hint.m_definition = "asset/objects/environment/label/label.object.json";
            level->createObject(Hint);
        }

        // 6:Generate the walls
        for (int i = 0; i < m_row; i++)
        {
            for (int j = 0; j < m_col; j++)
            {
                // Up
                if (!mazeDoors[i][j][0])
                {
                    int               wallNum = i * (2 * m_col + 1) + j;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    level->createObject(Wall);
                }
                // Left
                if (!mazeDoors[i][j][3])
                {
                    int               wallNum = i * (2 * m_col + 1) + j + m_col;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    level->createObject(Wall);
                }
                // Right
                if (j == m_col - 1)
                {
                    int               wallNum = i * (2 * m_col + 1) + j + m_col + 1;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    level->createObject(Wall);
                }
                // Down
                if (i == m_row - 1)
                {
                    int               wallNum = i * (2 * m_col + 1) + j + 2 * m_col + 1;
                    ObjectInstanceRes Wall;
                    Wall.m_name       = "Wall_" + std::to_string(wallNum);
                    Wall.m_definition = "asset/objects/environment/wall/wall.object.json";
                    level->createObject(Wall);
                }
            }
        }

        // 7:Place the object in the maze
        Vector3 startPosition;
        startPosition.x = -10 - 10 * (m_row - 1) / 2 + 5;
        startPosition.y = -10 * (m_col - 1) / 2;
        startPosition.z = 0;
        for (const auto& object_pair : level->m_gobjects)
        {
            std::shared_ptr<GObject> object = object_pair.second;
            if (object == nullptr)
            {
                continue;
            }
            if ("Player" == object->getName())
            {
                level->m_current_active_character       = std::make_shared<Character>(object);
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                transform_component->setPosition(startPosition);
                continue;
            }
            if ("Wall_" == (object->getName().substr(0, 5)))
            {
                int                 i                   = std::stoi(object->getName().substr(5));
                int                 rowNum              = int(i / (2 * m_col + 1));
                int                 colNum              = i % (2 * m_col + 1);
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                Vector3             new_translation;
                Quaternion          new_rotation;
                if (colNum < m_col)
                {
                    new_translation.x = -10 - 10 * (m_row - 1) / 2 + rowNum * 10;
                    new_translation.y = -10 * (m_col - 1) / 2 + colNum * 10;
                    new_translation.z = 0;
                }
                else
                {
                    colNum -= m_col;
                    new_translation.x = -5 - 10 * (m_row - 1) / 2 + rowNum * 10;
                    new_translation.y = -5 - 10 * (m_col - 1) / 2 + colNum * 10;
                    Vector3 axis(0, 0, 1);
                    Degree  d(90.0);
                    Radian  angle(d);
                    new_rotation.fromAngleAxis(angle, axis);
                }
                transform_component->setPosition(new_translation);
                transform_component->setRotation(new_rotation);
                continue;
            }
            if ("Hint_" == (object->getName().substr(0, 5)))
            {
                Vector3             new_translation;
                int                 i                   = std::stoi(object->getName().substr(5));
                auto                index               = m_path[i];
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                new_translation.x                       = startPosition.x + 10 * index.x;
                new_translation.y                       = startPosition.y + 10 * index.y;
                transform_component->setPosition(new_translation);
                continue;
            }
            if ("Ground_" == (object->getName().substr(0, 7)))
            {
                TransformComponent* transform_component = object->tryGetComponent(TransformComponent);
                int                 i                   = std::stoi(object->getName().substr(7));
                int                 wi                  = i % tiles_of_width;
                int                 li                  = i / tiles_of_width;

                Vector2 corner = {-10 - 10 * static_cast<float>(m_row - 1) / 2,
                                  -10 * static_cast<float>(m_col - 1) / 2 - 5};
                float   set_X  = corner.x + ground_W / 2.0;
                float   set_Y  = corner.y + ground_L / 2.0;
                set_X += ground_W * wi - (tiles_of_width * ground_W - width_of_maze) / 2.0;
                set_Y += ground_L * li - (tiles_of_length * ground_L - length_of_maze) / 2.0;

                Vector3 fixPosition(set_X, set_Y, 0);
                transform_component->setPosition(fixPosition);
                continue;
            }
        }
    }

    void MazeManager::generatePath(std::vector<std::vector<std::vector<bool>>>& mazeDoors,
                                   MazePositionIndex                            startPos,
                                   MazePositionIndex                            endPos)
    {
        // Initializes the data structure of the open close set in the maze
        std::priority_queue<MazeNode, std::vector<MazeNode>, std::greater<MazeNode>> open;
        std::unordered_set<MazePositionIndex>                                        close;
        std::unordered_map<MazePositionIndex, MazeNode>                              all_path; // cur node -> parent
        /*std::unordered_map<MazePositionIndex,MazePositionIndex> m_path*/

        std::vector<std::vector<bool>> openLUT(m_row, std::vector<bool>(m_col, true));

        auto startPoint = MazeNode(startPos, 0, 0);
        open.push(startPoint);
        close.clear();
        m_path.clear();
        openLUT[startPos.x][startPos.y] =
            false; // open priority queue Look Up Table for check whether this element is in the queue
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
                if (mazeDoors[mazeNodeTemp.index.x][mazeNodeTemp.index.y][i] && checkValid(MPI_temp) &&
                    close.find(MPI_temp) == close.end())
                {
                    MazeNode temp(MPI_temp, mazeNodeTemp.G + 1, ManhattanDis(endPos, MPI_temp));
                    if (openLUT[MPI_temp.x][MPI_temp.y])
                    {
                        openLUT[MPI_temp.x][MPI_temp.y] = false;
                        all_path[temp.index]            = mazeNodeTemp;
                        open.push(temp);
                    }
                    else
                    {
                        if (all_path[temp.index].G > temp.G)
                        {

                            all_path[temp.index].G     = temp.G;
                            all_path[temp.index].cost  = all_path[temp.index].G + all_path[temp.index].H;
                            all_path[temp.index].index = temp.index;
                        }
                    }
                }
            }
        }
        auto iter = endPos;
        while (iter != startPos)
        {
            m_path.push_back(iter);
            iter = all_path[iter].index;
        }
        m_path.push_back(startPos);
        std::reverse(m_path.begin(), m_path.end());
        LOG_INFO("Path generate success!");
    }

} // namespace Piccolo