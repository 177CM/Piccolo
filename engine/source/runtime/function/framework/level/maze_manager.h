#pragma once

#include "runtime/core/base/macro.h"
#include "runtime/core/math/math.h"

#include <memory>
#include <runtime/core/base/hash.h>
#include <vector>

namespace Piccolo
{
    class Level;
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

    class MazeManager
    {
    public:
        void generateMaze(std::shared_ptr<Level> level);

        void generatePath(std::vector<std::vector<std::vector<bool>>>& mazeDoors,
                          MazePositionIndex                            startPos,
                          MazePositionIndex                            endPos);

        inline void setRowAndCol(uint64_t row, uint64_t col)
        {
            m_row = row;
            m_col = col;
        }
        MazeManager() {}
        ~MazeManager() {}

        std::vector<MazePositionIndex> getMazePath() { return m_path; }

    private:
        bool getDir(const bool* mazeDoors, const MazePositionIndex& index, int dir)
        {
            return mazeDoors[index.x * m_col * 4 + index.y * 4 + dir];
        }
        bool checkValid(const MazePositionIndex& index)
        {
            return index.x >= 0 && index.x < m_row && index.y >= 0 && index.y < m_col;
        }
        int ManhattanDis(const MazePositionIndex& index_A, const MazePositionIndex& index_B)
        {
            return Math::abs(index_A.x - index_B.x) + Math::abs(index_A.y - index_B.y);
        }
        float GeometricDis(const MazePositionIndex& index_A, const MazePositionIndex& index_B)
        {
            return Math::sqrt(Math::sqr(index_A.x - index_B.x) + Math::sqr(index_A.y - index_B.y));
        }
        float CulculateCost(MazePositionIndex startPos, MazePositionIndex endPos, MazePositionIndex curIndex)
        {
            return GeometricDis(startPos, curIndex) + ManhattanDis(endPos, curIndex);
        }

        void clearCurrentLevel(std::shared_ptr<Level> level);

    private:
        uint64_t                       m_row = 0;
        uint64_t                       m_col = 0;
        std::vector<MazePositionIndex> m_path;
    };
} // namespace Piccolo