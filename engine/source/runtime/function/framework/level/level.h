#pragma once

#include "runtime/core/math/vector2.h"
#include "runtime/function/framework/object/object_id_allocator.h"

#include <memory>
#include <runtime/core/base/hash.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Piccolo
{
    struct MazePositionIndex
    {
        int               x, y;
        bool              operator==(const MazePositionIndex& other) const { return (x == other.x) && (y == other.y); }
        bool              operator!=(const MazePositionIndex& other) const { return (x != other.x) || (y != other.y); }
        MazePositionIndex operator+(const MazePositionIndex& other) const
        {
            MazePositionIndex result;
            result.x = x + other.x;
            result.y = y + other.y;
            return result;
        }
        MazePositionIndex(int _x, int _y) : x(_x), y(_y) {}
        MazePositionIndex() : x(0), y(0) {}
        bool operator<(const MazePositionIndex& rhs) const { return this->x < rhs.x; }
        bool operator>(const MazePositionIndex& rhs) const { return this->x > rhs.x; }
    };
    // struct MazePositionIndexHash
    // {//use at unordered_set hash operation
    //     size_t operator()(Piccolo::MazePositionIndex const &posIndex) const
    //     {
    //     size_t seed = 0;
    //     hash_combine(seed, posIndex.x,posIndex.y);
    //     return seed;
    //     }
    // };

    struct MazeNode
    {
        MazePositionIndex index;
        int               G;
        int               H;
        int               cost;
        bool              operator<(const MazeNode& rhs) const { return this->cost < rhs.cost; }
        bool              operator>(const MazeNode& rhs) const { return this->cost > rhs.cost; }
        MazeNode() : G(0), H(0), cost(0) {}
        MazeNode(const MazePositionIndex& _index, int _G, int _H) : index(_index), G(_G), H(_H), cost(_G + _H) {}
    };
} // namespace Piccolo
namespace std
{
    template<>
    struct hash<Piccolo::MazePositionIndex>
    {
        size_t operator()(Piccolo::MazePositionIndex const& posIndex) const
        {
            size_t seed = 0;
            hash_combine(seed, posIndex.x, posIndex.y);
            return seed;
        }
    };
    template<>
    struct hash<Piccolo::MazeNode>
    {
        size_t operator()(Piccolo::MazeNode const& mazeNode) const
        {
            size_t seed = 0;
            hash_combine(seed, mazeNode.index, mazeNode.cost);
            return seed;
        }
    };
} // namespace std
namespace Piccolo
{
    class Character;
    class GObject;
    class ObjectInstanceRes;
    class PhysicsScene;
    using LevelObjectsMap = std::unordered_map<GObjectID, std::shared_ptr<GObject>>;

    /// The main class to manage all game objects
    class Level
    {
    public:
        virtual ~Level() {};

        bool load(const std::string& level_res_url);
        void unload();

        bool save();

        void tick(float delta_time);

        const std::string& getLevelResUrl() const { return m_level_res_url; }

        const LevelObjectsMap& getAllGObjects() const { return m_gobjects; }

        std::weak_ptr<GObject>   getGObjectByID(GObjectID go_id) const;
        std::weak_ptr<Character> getCurrentActiveCharacter() const { return m_current_active_character; }

        GObjectID createObject(const ObjectInstanceRes& object_instance_res);
        void      deleteGObjectByID(GObjectID go_id);

        std::weak_ptr<PhysicsScene> getPhysicsScene() const { return m_physics_scene; }

        void generateMaze();

        void generatePath(bool*             mazeDoors,
                          const int&        rows,
                          const int&        cols,
                          MazePositionIndex startPos,
                          MazePositionIndex endPos);

        std::vector<MazePositionIndex> getMazePath() { return m_path; }

    protected:
        void clear();

        bool        m_is_loaded {false};
        std::string m_level_res_url;

        // all game objects in this level, key: object id, value: object instance
        LevelObjectsMap m_gobjects;

        std::shared_ptr<Character> m_current_active_character;

        std::weak_ptr<PhysicsScene> m_physics_scene;

        std::vector<MazePositionIndex> m_path;
    };
} // namespace Piccolo
