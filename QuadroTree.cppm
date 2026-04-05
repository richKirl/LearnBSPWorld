// QuadroTree.cppm
module; // Начало глобального фрагмента


#include <cmath>
#include <memory>
#include <vector>
export module QuadroTree;


export struct XYZ {
    float x, y, z;
    XYZ(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

export enum class ActorType { Player, Monster, Prop, Npc };

export struct Actor {
    uint64_t id;
    ActorType type;
    XYZ position;
    float health;
    // Можно добавить метаданные (имя, уровень и т.д.)
};


export struct BBox {
    XYZ center;
    XYZ halfDim; // Радиус куба по осям

    bool contains(const XYZ& p) const {
        return (p.x >= center.x - halfDim.x && p.x <= center.x + halfDim.x &&
                p.y >= center.y - halfDim.y && p.y <= center.y + halfDim.y &&
                p.z >= center.z - halfDim.z && p.z <= center.z + halfDim.z);
    }

    bool intersects(const BBox& other) const {
        return (std::abs(center.x - other.center.x) < (halfDim.x + other.halfDim.x) &&
                std::abs(center.y - other.center.y) < (halfDim.y + other.halfDim.y) &&
                std::abs(center.z - other.center.z) < (halfDim.z + other.halfDim.z));
    }
};





export class Octree {
    constexpr static int CAPACITY = 8;
    BBox boundary;
    std::vector<Actor*> actors;
    std::unique_ptr<Octree> children[8]; // 8 подобластей

public:
    Octree(BBox b) : boundary(b) {
        for (int i = 0; i < 8; ++i) children[i] = nullptr;
    }

    void subdivide() {
        float hx = boundary.halfDim.x / 2.0f;
        float hy = boundary.halfDim.y / 2.0f;
        float hz = boundary.halfDim.z / 2.0f;

        // Генерация 8 центров для новых кубов
        for (int i = 0; i < 8; ++i) {
            XYZ newCenter = {
                boundary.center.x + hx * ((i & 1) ? 1.0f : -1.0f),
                boundary.center.y + hy * ((i & 2) ? 1.0f : -1.0f),
                boundary.center.z + hz * ((i & 4) ? 1.0f : -1.0f)
            };
            children[i] = std::make_unique<Octree>(BBox{newCenter, {hx, hy, hz}});
        }
    }

    bool insert(Actor* actor) {
        if (!boundary.contains(actor->position)) return false;
        if (actors.size() < CAPACITY && children[0] == nullptr) {
            actors.push_back(actor);
            return true;
        }
        if (children[0] == nullptr) subdivide();
        for (int i = 0; i < 8; ++i) if (children[i]->insert(actor)) return true;
        return false;
    }

    // query пишется аналогично insert, проходя по всем 8 детям
    void query(const BBox& range, std::vector<Actor*>& found) const {
        // Если область поиска не пересекается с кубом — выходим
        if (!boundary.intersects(range)) return;

        // Проверяем акторов в текущем узле
        for (auto* actor : actors) {
            if (range.contains(actor->position)) {
                found.push_back(actor);
            }
        }

        // Рекурсивно идем в детей
        if (children[0] != nullptr) {
            for (int i = 0; i < 8; ++i) {
                children[i]->query(range, found);
            }
        }
    }

    void clear() {
        actors.clear();
        for (int i = 0; i < 8; ++i) {
            if (children[i]) {
                children[i]->clear(); // Рекурсивный сброс
                children[i].reset();  // Удаление веток
            }
        }
    }
};



export struct WorldTile {
    int tx, ty;
    std::unique_ptr<Octree> tree;

    WorldTile(int x, int y) : tx(x), ty(y) {
        // Каждый тайл 500x500, высота 1000 (для гор и пещер)
        BBox area = {{(float)x * 500 + 250, (float)y * 500 + 250, 0}, {250, 250, 500}};
        tree = std::make_unique<Octree>(area);
    }
};

export class WorldManager {
    std::vector<std::unique_ptr<WorldTile>> tiles;
public:
    WorldManager() {
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 10; ++x)
                tiles.push_back(std::make_unique<WorldTile>(x, y));
    }

    bool addActor(Actor* actor) {
        // Определяем индексы тайла
        int ix = static_cast<int>(actor->position.x / 500);
        int iy = static_cast<int>(actor->position.y / 500);

        // Проверка границ мира (0..9)
        if (ix >= 0 && ix < 10 && iy >= 0 && iy < 10) {
            return tiles[iy * 10 + ix]->tree->insert(actor);
        }
        return false;
    }

    void getActorsInRange(const BBox& range, std::vector<Actor*>& out) {
        // В идеале: определить, какие тайлы задевает range, и спросить только их.
        // Пока для простоты спросим все:
        for (auto& tile : tiles) {
            tile->tree->query(range, out);
        }
    }
};
