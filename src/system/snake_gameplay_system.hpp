#ifndef SRC_SYSTEM_SNAKE_DISCRETE_VECTOR_HPP
#define SRC_SYSTEM_SNAKE_DISCRETE_VECTOR_HPP

#include <vector>
#include <string>

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <entt/entt.hpp>
#include <sigslot/signal.hpp>

#include <component/velocity.hpp>
#include <component/key_control.hpp>
#include <component/snake_apple.hpp>
#include <component/snake_part.hpp>
#include <component/snake_part_head.hpp>
#include <component/snake_boundary_2d.hpp>

namespace SnakeGameplaySystem
{
    enum MapSlotState : Uint8
    {
        EMPTY = 0b0000U,
        SNAKE_HEAD = 0b0001U,
        SNAKE_BODY = 0b0010U,
        APPLE = 0b0100U,

        ENUM_END = 0b1111U,
    }; // enum MapSlotState

    inline std::vector<std::vector<MapSlotState>> previousMap;

    namespace Control
    {
        static void shift_key_up(entt::registry &reg);
        static void shift_key_down(entt::registry &reg);
        static void up_key_down(entt::registry &reg);
        static void left_key_down(entt::registry &reg);
        static void down_key_down(entt::registry &reg);
        static void right_key_down(entt::registry &reg);
    } // namespace Control

    namespace Util
    {
        static void get_index_from_pos(const Position &pos, long *x, long *y, const long &sizeY);
        static Position get_pos_from_index(const long &x, const long &y, const long &sizeY);
    } // namespace Util

    namespace Detail
    {
        static bool is_going_backwards(entt::registry &reg, const char &directionToGo);
        static void do_trailing(entt::registry &reg, const bool &isAteApple);
        static bool apple_update(entt::registry &reg);
    } // namespace Detail

    static std::vector<std::vector<MapSlotState>> get_map(entt::registry &reg);
    static bool is_game_success(entt::registry &reg);
    static bool is_game_failure(entt::registry &reg);
    static unsigned long get_score(entt::registry &reg);
    static bool is_speeding_up(entt::registry &reg);

    static void iterate(entt::registry &reg)
    {
        if (is_game_success(reg))
            return;
        if (is_game_failure(reg))
            return;

        const bool ateApple = Detail::apple_update(reg);

        auto keyControlView = reg.view<KeyControl>();
        SDL_assert(keyControlView.size() == 1);
        KeyControl keyControl = reg.get<KeyControl>(keyControlView.front());

        auto snakeHeadView = reg.view<Velocity, SnakePartHead>();
        SDL_assert(snakeHeadView.storage<SnakePartHead>()->size() == 1);
        for (auto &entity : snakeHeadView)
        {
            Velocity &vel = snakeHeadView.get<Velocity>(entity);
            SnakePartHead &headPart = snakeHeadView.get<SnakePartHead>(entity);
            switch (keyControl.lastMovementKeyDown)
            {
            case 'w':
                if (Detail::is_going_backwards(reg, 'w'))
                    break;
                vel.x = 0.0f;
                vel.y = headPart.speed;
                if (keyControl.isShiftKeyDown)
                    vel.y *= headPart.speedUpFactor;
                break;
            case 'a':
                if (Detail::is_going_backwards(reg, 'a'))
                    break;
                vel.x = -1.0f * headPart.speed;
                if (keyControl.isShiftKeyDown)
                    vel.x *= headPart.speedUpFactor;
                vel.y = 0.0f;
                break;
            case 's':
                if (Detail::is_going_backwards(reg, 's'))
                    break;
                vel.x = 0.0f;
                vel.y = -1.0f * headPart.speed;
                if (keyControl.isShiftKeyDown)
                    vel.y *= headPart.speedUpFactor;
                break;
            case 'd':
                if (Detail::is_going_backwards(reg, 'd'))
                    break;
                vel.x = headPart.speed;
                if (keyControl.isShiftKeyDown)
                    vel.x *= headPart.speedUpFactor;
                vel.y = 0.0f;
                break;
            default:
                break;
            }
        }
        previousMap = get_map(reg);

        const SnakeBoundary2D boundary = reg.get<SnakeBoundary2D>(reg.view<SnakeBoundary2D>().front());
        for (int i = 0; i < previousMap.size(); i++)
        {
            for (int j = 0; j < previousMap[i].size(); j++)
            {
                if ((previousMap[i][j] & MapSlotState::SNAKE_HEAD) && (previousMap[i][j] & MapSlotState::SNAKE_BODY))
                {
                    auto snakePartView = reg.view<SnakePart>();
                    for (auto &entity : snakePartView)
                    {
                        const Position pos = reg.get<Position>(entity);
                        long xIndex, yIndex;
                        Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                        if (xIndex == j && yIndex == i)
                            reg.destroy(entity);
                    }
                }
            }
        }
    }
    static void update(entt::registry &reg) { return iterate(reg); }

    static bool init(entt::registry &reg)
    {
        {
            auto snakeHeadView = reg.view<SnakePartHead>();
            if (snakeHeadView.empty())
                return false;
        }
        previousMap = get_map(reg);
        return true;
    }
    static bool init(sigslot::signal<entt::registry &> &signal, entt::registry &reg)
    {
        static std::list<sigslot::signal<entt::registry &> *> regSignalArray;
        bool ret = true;
        for (auto connectedSignal : regSignalArray)
        {
            if (connectedSignal == &signal)
            {
                ret = false;
                break;
            }
        }
        if (ret)
        {
            signal.connect(SnakeGameplaySystem::iterate);
            regSignalArray.push_back(&signal);
            init(reg);
        }
        return ret;
    }

    static std::vector<std::vector<MapSlotState>> get_map(entt::registry &reg)
    {
        auto snakeBoundaryView = reg.view<SnakeBoundary2D>();
        SDL_assert(snakeBoundaryView.size() == 1);
        const SnakeBoundary2D boundary = reg.get<SnakeBoundary2D>(snakeBoundaryView.front());

        const int xSize = boundary.x;
        const int ySize = boundary.y;

        std::vector<std::vector<MapSlotState>> ret(ySize, std::vector<MapSlotState>(xSize, MapSlotState::EMPTY));

        auto snakePartView = reg.view<SnakePart, Position>();
        for (auto &entity : snakePartView)
        {
            const Position &pos = snakePartView.get<Position>(entity);
            long xIndex, yIndex;
            SnakeGameplaySystem::Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
            if (xIndex >= 0 && yIndex >= 0 && yIndex < ret.size() && xIndex < ret[yIndex].size())
                ret[yIndex][xIndex] = MapSlotState::SNAKE_BODY;
        }

        auto snakePartHeadView = reg.view<SnakePartHead, Position>();
        for (auto &entity : snakePartHeadView)
        {
            const Position &pos = snakePartView.get<Position>(entity);
            long xIndex, yIndex;
            SnakeGameplaySystem::Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
            if (xIndex >= 0 && yIndex >= 0 && yIndex < ret.size() && xIndex < ret[yIndex].size())
            {
                Uint8 entry = static_cast<Uint8>(ret[yIndex][xIndex]) | MapSlotState::SNAKE_HEAD;
                ret[yIndex][xIndex] = static_cast<MapSlotState>(entry);
            }
        }

        auto appleView = reg.view<SnakeApple, Position>();
        for (auto &entity : appleView)
        {
            const Position &pos = appleView.get<Position>(entity);
            long xIndex, yIndex;
            SnakeGameplaySystem::Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
            if (xIndex >= 0 && yIndex >= 0 && yIndex < ret.size() && xIndex < ret[yIndex].size())
            {
                Uint8 entry = static_cast<Uint8>(ret[yIndex][xIndex]) | MapSlotState::APPLE;
                ret[yIndex][xIndex] = static_cast<MapSlotState>(entry);
            }
        }

        return ret;
    }
    static bool is_game_success(entt::registry &reg)
    {
        auto map = get_map(reg);
        for (int i = 0; i < map.size(); i++)
        {
            for (int j = 0; j < map[i].size(); j++)
            {
                if (map[i][j] == MapSlotState::EMPTY || map[i][j] == MapSlotState::APPLE)
                    return false;
            }
        }
        return true;
    }
    static bool is_game_failure(entt::registry &reg)
    {
        auto snakeHeadPos = reg.get<Position>(reg.view<SnakePartHead, Position>().front());
        auto boundary = reg.get<SnakeBoundary2D>(reg.view<SnakeBoundary2D>().front());
        if (snakeHeadPos.x < 0.0f || snakeHeadPos.x >= boundary.x || snakeHeadPos.y < 0.0f || snakeHeadPos.y >= boundary.y)
            return true;

        auto map = get_map(reg);
        for (int i = 0; i < map.size(); i++)
        {
            for (int j = 0; j < map[i].size(); j++)
            {
                if ((static_cast<Uint8>(map[i][j]) & SNAKE_HEAD) && (static_cast<Uint8>(map[i][j]) & SNAKE_BODY))
                { // TODO: refactor below and also the same code to find tail in Detail::do_trailing()
                    auto snakePartView = reg.view<Position, SnakePart>();
                    struct Index
                    {
                        explicit Index(const int &_i, const int &_j) : i(_i), j(_j) {}
                        int i;
                        int j;
                    }; // struct Index
                    std::vector<Index> indexVec;
                    for (const auto &entity : snakePartView)
                    {
                        const bool isValid = reg.all_of<SnakePart, Position>(entity);
                        SDL_assert(isValid);
                        Position pos = reg.get<Position>(entity);
                        const SnakePart snakePart = reg.get<SnakePart>(entity);
                        switch (snakePart.currentDirection)
                        {
                        case 'w':
                        {
                            pos.y += 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        case 'a':
                        {
                            pos.x -= 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        case 's':
                        {
                            pos.y -= 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        case 'd':
                        {
                            pos.x += 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        }
                    }
                    // Now that the pool of indices of next parts are gotten,
                    // look for the 1 part that doesn't have index within the pool.
                    bool hasFoundTail = false;
                    for (auto &entity : snakePartView)
                    {
                        const Position pos = reg.get<Position>(entity);
                        long xIndex, yIndex;
                        Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                        bool isInPool = false;
                        for (Index index : indexVec)
                        {
                            if (index.i == yIndex && index.j == xIndex)
                            {
                                isInPool = true;
                                break;
                            }
                        }
                        if (!isInPool)
                        {
                            hasFoundTail = true;
                            const Position tailPos = reg.get<Position>(entity);
                            long xIndex, yIndex;
                            Util::get_index_from_pos(tailPos, &xIndex, &yIndex, boundary.y);
                            if (xIndex == j && yIndex == i) // this means it's the tail
                                return false;
                            else
                                return true;
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }
    static unsigned long get_score(entt::registry &reg) { return reg.view<SnakePart>().size(); }
    static bool is_speeding_up(entt::registry &reg) { return reg.get<KeyControl>(reg.view<KeyControl>().front()).isShiftKeyDown; }

    namespace Detail
    {
        static bool is_going_backwards(entt::registry &reg, const char &directionToGo)
        {
            struct Index
            {
                explicit Index(const int &_i, const int &_j) : i(_i), j(_j) {}
                int i;
                int j;
                bool isInvalid() { return i < 0 || j < 0; }
            };
            auto map = get_map(reg);
            Index indexOfSnakeHead(-1, -1);
            for (int i = 0; i < map.size(); i++)
            {
                for (int j = 0; j < map[i].size(); j++)
                {
                    if (map[i][j] == MapSlotState::SNAKE_HEAD)
                        indexOfSnakeHead = Index(i, j);
                }
            }
            if (indexOfSnakeHead.isInvalid())
                return true;

            int i = indexOfSnakeHead.i, j = indexOfSnakeHead.j;
            switch (directionToGo)
            { // check if it's a wall or not a snake body
            case 'w':
                if (i == 0)
                    return false;
                if (map[i - 1][j] != MapSlotState::SNAKE_BODY)
                    return false;
                break;
            case 'a':
                if (j == 0)
                    return false;
                if (map[i][j - 1] != MapSlotState::SNAKE_BODY)
                    return false;
                break;
            case 's':
                if (i == map.size() - 1)
                    return false;
                if (map[i + 1][j] != MapSlotState::SNAKE_BODY)
                    return false;
                break;
            case 'd':
                if (j == map[i].size() - 1)
                    return false;
                if (map[i][j + 1] != MapSlotState::SNAKE_BODY)
                    return false;
                break;
            default:
                return true;
            }

            // It's a snake body at the directionToGo. Find out if it's
            // the "neck". If it's not, return false.
            auto snakeBoundaryView = reg.view<SnakeBoundary2D>();
            SDL_assert(snakeBoundaryView.size() == 1);
            const SnakeBoundary2D boundary = reg.get<SnakeBoundary2D>(snakeBoundaryView.front());

            auto view = reg.view<Position, SnakePart>();
            for (auto &entity : view)
            {
                const Position &pos = reg.get<Position>(entity);
                long xIndex, yIndex;
                Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                switch (directionToGo)
                {
                // Get the body part in that direction and get its currentDirection.
                // If the currentDirection is opposite of directionToGo, return true.
                case 'w':
                {
                    if (xIndex == j && yIndex == i - 1 && reg.get<SnakePart>(entity).currentDirection == 's')
                        return true;
                    break;
                }
                case 'a':
                {
                    if (xIndex == j - 1 && yIndex == i && reg.get<SnakePart>(entity).currentDirection == 'd')
                        return true;
                    break;
                }
                case 's':
                {
                    if (xIndex == j && yIndex == i + 1 && reg.get<SnakePart>(entity).currentDirection == 'w')
                        return true;
                    break;
                }
                case 'd':
                {
                    if (xIndex == j + 1 && yIndex == i && reg.get<SnakePart>(entity).currentDirection == 'a')
                        return true;
                    break;
                }
                default:
                    SDL_assert(directionToGo == 'w' || directionToGo == 'a' || directionToGo == 's' || directionToGo == 'd');
                    return true;
                }
            }
            return false;
        }
        static void do_trailing(entt::registry &reg, const bool &isAteApple)
        { // NOTE: this function is the reason why the update loop NEEDS to limit DeltaTime
            auto currentMap = get_map(reg);
            if (currentMap == previousMap)
                return;

            struct Index
            {
                explicit Index(const int &_i, const int &_j) : i(_i), j(_j) {}
                int i;
                int j;
            }; // struct Index

            auto getSnakeHeadIndices = [](const std::vector<std::vector<MapSlotState>> &map)
            {
                Index ret(-1, -1);
                for (int i = 0; i < map.size(); i++)
                {
                    for (int j = 0; j < map[i].size(); j++)
                    {
                        if (map[i][j] & MapSlotState::SNAKE_HEAD)
                            return Index(i, j);
                    }
                }
                return ret;
            };

            Index previousSnakeHeadIndex = getSnakeHeadIndices(previousMap);
            Index currentSnakeHeadIndex = getSnakeHeadIndices(currentMap);

            if (previousSnakeHeadIndex.i == currentSnakeHeadIndex.i && previousSnakeHeadIndex.j == currentSnakeHeadIndex.j)
                return;

            char travelledDirection = '\t';
            if (currentSnakeHeadIndex.i < previousSnakeHeadIndex.i)
                travelledDirection = 'w';
            else if (currentSnakeHeadIndex.j < previousSnakeHeadIndex.j)
                travelledDirection = 'a';
            else if (currentSnakeHeadIndex.i > previousSnakeHeadIndex.i)
                travelledDirection = 's';
            else if (currentSnakeHeadIndex.j > previousSnakeHeadIndex.j)
                travelledDirection = 'd';

            SDL_assert(travelledDirection != '\t');
            if (travelledDirection == '\t')
                return;

            auto snakePartView = reg.view<SnakePart>();
            if (snakePartView.empty())
            {
                if (!isAteApple)
                    return;

                // Ate apple, so spawn a part behind the snake head.
                auto snakeBoundaryView = reg.view<SnakeBoundary2D>();
                SDL_assert(snakeBoundaryView.size() == 1);
                const SnakeBoundary2D boundary = reg.get<SnakeBoundary2D>(snakeBoundaryView.front());
                const int i = currentSnakeHeadIndex.i, j = currentSnakeHeadIndex.j;
                switch (travelledDirection)
                {
                case 'w':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 'w');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(j, i + 1, boundary.y));
                    break;
                }
                case 'a':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 'a');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(j + 1, i, boundary.y));
                    break;
                }
                case 's':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 's');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(j, i - 1, boundary.y));
                    break;
                }
                case 'd':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 'd');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(j - 1, i, boundary.y));
                    break;
                }
                } // switch (travelledDirection)
            }
            else
            {
                auto snakeBoundaryView = reg.view<SnakeBoundary2D>();
                SDL_assert(snakeBoundaryView.size() == 1);
                const SnakeBoundary2D boundary = reg.get<SnakeBoundary2D>(snakeBoundaryView.front());
                int i = currentSnakeHeadIndex.i, j = currentSnakeHeadIndex.j;
                switch (travelledDirection)
                { // spawn in neck part
                case 'w':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 'w');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(j, ++i, boundary.y));
                    break;
                }
                case 'a':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 'a');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(++j, i, boundary.y));
                    break;
                }
                case 's':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 's');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(j, --i, boundary.y));
                    break;
                }
                case 'd':
                {
                    auto entitySnakePart = reg.create();
                    reg.emplace<SnakePart>(entitySnakePart, 'd');
                    reg.emplace<Position>(entitySnakePart, Util::get_pos_from_index(--j, i, boundary.y));
                    break;
                }
                } // switch (travelledDirection)

                if (!isAteApple)
                {
                    // At this point, i and j are at the neck (already spawned in).
                    // Since no apple is eaten, we need to destroy the tail.
                    // We can find the tail by looking at all the SnakePart
                    // currentDirection as well as their Position to determine
                    // where the next part should be. Have a pool of these next part
                    // indices. The tail is the one that has a pos not in that pool.
                    auto snakePartView = reg.view<Position, SnakePart>();
                    std::vector<Index> indexVec;
                    for (const auto &entity : snakePartView)
                    {
                        const bool isValid = reg.all_of<SnakePart, Position>(entity);
                        SDL_assert(isValid);
                        Position pos = reg.get<Position>(entity);
                        const SnakePart snakePart = reg.get<SnakePart>(entity);
                        switch (snakePart.currentDirection)
                        {
                        case 'w':
                        {
                            pos.y += 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        case 'a':
                        {
                            pos.x -= 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        case 's':
                        {
                            pos.y -= 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        case 'd':
                        {
                            pos.x += 1.0f;
                            long xIndex, yIndex;
                            Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                            if (xIndex >= 0 && xIndex < boundary.x && yIndex >= 0 && yIndex < boundary.y)
                                indexVec.push_back(Index(yIndex, xIndex));
                            break;
                        }
                        }
                    }
                    // Now that the pool of indices of next parts are gotten,
                    // look for the 1 part that doesn't have index within the pool.
                    bool hasFoundTail = false;
                    for (auto &entity : snakePartView)
                    {
                        const Position pos = reg.get<Position>(entity);
                        long xIndex, yIndex;
                        Util::get_index_from_pos(pos, &xIndex, &yIndex, boundary.y);
                        bool isInPool = false;
                        for (Index index : indexVec)
                        {
                            if (index.i == yIndex && index.j == xIndex)
                            {
                                isInPool = true;
                                break;
                            }
                        }
                        if (!isInPool)
                        {
                            hasFoundTail = true;
                            reg.destroy(entity);
                            break;
                        }
                    }
                    SDL_assert(hasFoundTail);
                }
            }
        }
        static bool apple_update(entt::registry &reg)
        {
            auto map = get_map(reg);
            struct Index
            {
                explicit Index(const int &_i, const int &_j) : i(_i), j(_j) {}
                int i;
                int j;
            };
            std::vector<Index> indexVec;
            bool isEaten = false;
            for (int i = 0; i < map.size(); i++)
            {
                for (int j = 0; j < map[i].size(); j++)
                {
                    if (map[i][j] == MapSlotState::EMPTY)
                        indexVec.push_back(Index(i, j));
                    else if ((map[i][j] & MapSlotState::SNAKE_HEAD) && (map[i][j] & MapSlotState::APPLE))
                    {
                        isEaten = true;
                    }
                }
            }
            Detail::do_trailing(reg, isEaten);

            auto respawnApple = [](entt::registry &reg, const std::vector<Index> &indexVec)
            {
                auto appleView = reg.view<SnakeApple, Position>();
                SDL_assert(appleView.storage<SnakeApple>()->size() <= 1);
                const bool isNoApple = appleView.storage<SnakeApple>()->empty();
                if (!isNoApple) // don't spawn in when there's no apple
                {
                    if (indexVec.empty())
                        reg.destroy(appleView.front());
                    else
                    {
                        Position &applePos = reg.get<Position>(appleView.front());
                        const Sint32 indexVexIndex = SDL_rand(indexVec.size());
                        const int y = indexVec[indexVexIndex].i;
                        const int x = indexVec[indexVexIndex].j;

                        auto snakeBoundaryView = reg.view<SnakeBoundary2D>();
                        SDL_assert(snakeBoundaryView.size() == 1);
                        const SnakeBoundary2D boundary = reg.get<SnakeBoundary2D>(snakeBoundaryView.front());
                        applePos = Util::get_pos_from_index(x, y, boundary.y);
                    }
                }
            };

            if (isEaten)
            {
                respawnApple(reg, indexVec);
                map = get_map(reg);

                bool hasErased = false;
                for (auto it = indexVec.begin(); it != indexVec.end();) // search for conflicting spots since the head was detached
                {                                                       // solves apple at neck issue
                    Index index = *it;
                    if ((map[index.i][index.j] & MapSlotState::APPLE) && map[index.i][index.j] > MapSlotState::APPLE)
                    {
                        hasErased = true;
                        it = indexVec.erase(it); // erase() returns iterator to next element
                    }
                    else
                        ++it;
                }
                if (hasErased)
                    respawnApple(reg, indexVec);
            }

            return isEaten;
        }
    } // namespace Detail

    namespace Control
    {
        static void shift_key_up(entt::registry &reg)
        {
            auto keyControlView = reg.view<KeyControl>();
            SDL_assert(keyControlView.size() == 1);
            keyControlView.each([](KeyControl &control)
                                { control.isShiftKeyDown = false; });
        }
        static void shift_key_down(entt::registry &reg)
        {
            auto keyControlView = reg.view<KeyControl>();
            SDL_assert(keyControlView.size() == 1);
            keyControlView.each([](KeyControl &control)
                                { control.isShiftKeyDown = true; });
        }
        static void up_key_down(entt::registry &reg)
        {
            auto keyControlView = reg.view<KeyControl>();
            SDL_assert(keyControlView.size() == 1);
            keyControlView.each([](KeyControl &control)
                                { control.lastMovementKeyDown = 'w'; });
        }
        static void left_key_down(entt::registry &reg)
        {
            auto keyControlView = reg.view<KeyControl>();
            SDL_assert(keyControlView.size() == 1);
            keyControlView.each([](KeyControl &control)
                                { control.lastMovementKeyDown = 'a'; });
        }
        static void down_key_down(entt::registry &reg)
        {
            auto keyControlView = reg.view<KeyControl>();
            SDL_assert(keyControlView.size() == 1);
            keyControlView.each([](KeyControl &control)
                                { control.lastMovementKeyDown = 's'; });
        }
        static void right_key_down(entt::registry &reg)
        {
            auto keyControlView = reg.view<KeyControl>();
            SDL_assert(keyControlView.size() == 1);
            keyControlView.each([](KeyControl &control)
                                { control.lastMovementKeyDown = 'd'; });
        }
    } // namespace Control

    namespace Util
    {
        static void get_index_from_pos(const Position &pos, long *x, long *y, const long &sizeY)
        {
            // value >= 0.0f and value < 1.0f is inside of 1
            // if < 0.0f, it is 0; if >= 1.0f, it is 2
            SDL_assert(x != nullptr && y != nullptr);
            if (x != nullptr)
            {
                *x = SDL_lroundf(SDL_ceilf(pos.x));
                if (pos.x == *x)
                    *x += 1L;
                *x = *x - 1;
            }
            if (y != nullptr)
            {
                *y = SDL_lroundf(SDL_ceilf(pos.y));
                if (pos.y == *y)
                    *y += 1L;
                *y = sizeY - *y;
                if (sizeY == 1) // solves segfault
                    *y = 0;
            }
        }

        static Position get_pos_from_index(const long &x, const long &y, const long &sizeY)
        {
            Position ret;
            ret.x = static_cast<float>(x + 1L) - 0.5f;
            ret.y = static_cast<float>(sizeY - y) - 0.5f;
            return ret;
        }
    } // namespace Util

    namespace Debug
    {
        static Position get_snake_head_pos(const entt::registry &reg)
        {
            auto view = reg.view<Position, SnakePartHead>();
            SDL_assert(view.storage<SnakePartHead>()->size() == 1);
            return reg.get<Position>(view.front());
        }
        static Velocity get_snake_head_velocity(const entt::registry &reg)
        {
            auto view = reg.view<Velocity, SnakePartHead>();
            SDL_assert(view.storage<SnakePartHead>()->size() == 1);
            return reg.get<Velocity>(view.front());
        }

        static void print_map(const std::vector<std::vector<MapSlotState>> &map)
        {
            std::string str = "";
            for (int i = 0; i < map.size(); i++)
            {
                for (int j = 0; j < map[i].size(); j++)
                {
                    if (static_cast<Uint8>(map[i][j]) == EMPTY)
                        str += ".";
                    if (static_cast<Uint8>(map[i][j]) & SNAKE_HEAD)
                        str += "$";
                    if (static_cast<Uint8>(map[i][j]) & SNAKE_BODY)
                        str += "x";
                    if (static_cast<Uint8>(map[i][j]) & APPLE)
                        str += "@";
                    str += " "; // allows manual checking for collisions
                }
                if (i < map.size() - 1)
                    str += "\n\t";
            }
            SDL_Log("\t%s", str.c_str());
        }

        static void print_snake_head_pos(const entt::registry &reg)
        {
            Position pos = get_snake_head_pos(reg);
            SDL_Log("\tSnakeHead is at Position(%f, %f)", pos.x, pos.y);
        }
        static void print_snake_head_vel(const entt::registry &reg)
        {
            Velocity vel = get_snake_head_velocity(reg);
            SDL_Log("\tSnakeHead is at Velocity(%f, %f)", vel.x, vel.y);
        }
    } // namespace Debug
} // namespace SnakeDiscreteVector

#endif // SRC_SYSTEM_SNAKE_DISCRETE_VECTOR_HPP
