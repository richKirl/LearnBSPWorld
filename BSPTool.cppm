module;
#include <GL/glew.h>
#include <cmath>      // Импортируем стандартные библиотеки
#include <algorithm>
#include <vector>
#include <map>
#include <fstream>
#include <concepts>
export module BSPTool;
export import MathGeometry;
export template<Numeric T>
struct Plane {
    Vec3<T> normal; // Куда "смотрит" плоскость
    T distance;     // Расстояние от начала координат
    float getDistance(const Vec3<T>& point) const {
        return point.dot(normal) - distance;
    }
    // Классическая функция Кармака: определение стороны
    // Используется для обхода BSP дерева и коллизий
    int classifyPoint(const Vec3<T>& point) const {
        T result = point.dot(normal) - distance;
        if (result > 0.001f) return 1;  // FRONT
        if (result < -0.001f) return -1; // BACK
        return 0;                        // ON PLANE
    }
};


// Функция создания плоскости по 3 точкам (классика Quake)
// Направление нормали зависит от порядка обхода (обычно против часовой стрелки)
export template<Numeric T>
Plane<T> planeFromPoints(Vec3<T> p1, Vec3<T> p2, Vec3<T> p3) {
    Vec3<T> v1 = p2 + (p1 * -1); // Вектор p1 -> p2
    Vec3<T> v2 = p3 + (p1 * -1); // Вектор p1 -> p3

    Vec3<T> normal = v1.cross(v2).normalize();
    T distance = normal.dot(p1);

    return {normal, distance};
}
enum class NodeType { Internal, Leaf };

export template<Numeric T>
struct BSPNode {
    NodeType type = NodeType::Internal;
    Plane<T> splitPlane;

    std::unique_ptr<BSPNode> front;
    std::unique_ptr<BSPNode> back;

    bool isSolid = false; // Только для листьев: стена это или пустота
};
export template<Numeric T>
std::unique_ptr<BSPNode<T>> buildBSP(std::vector<Plane<T>> planes) {
    if (planes.empty()) {
        auto leaf = std::make_unique<BSPNode<T>>();
        leaf->type = NodeType::Leaf;
        leaf->isSolid = true; // Упрощенно: если нет плоскостей, мы внутри стены
        return leaf;
    }

    // 1. Выбираем разделитель (в идеале — тот, что меньше всего режет другие)
    Plane<T> rootPlane = planes[0];
    auto node = std::make_unique<BSPNode<T>>();
    node->splitPlane = rootPlane;

    std::vector<Plane<T>> frontList, backList;

    // 2. Распределяем остальные плоскости
    for (size_t i = 1; i < planes.size(); ++i) {
        // Здесь должна быть логика классификации полигона относительно плоскости
        // Если плоскость пересекает другую — её нужно "разрезать" (сложная часть)
        // Для начала просто раскидаем по центроидам
        int side = node->splitPlane.classifyPoint(planes[i].normal * planes[i].distance);

        if (side >= 0) frontList.push_back(planes[i]);
        else backList.push_back(planes[i]);
    }

    // 3. Рекурсия
    node->front = buildBSP(std::move(frontList));
    node->back = buildBSP(std::move(backList));

    return node;
}
// Структура для хранения грани (полигона)
export struct Face {
    std::vector<Vec3<float>> vertices;
    Plane<float> plane;
};

// Функция создания "базового" огромного многоугольника для плоскости
export Face createHugePoly(const Plane<float>& plane) {
    // 1. Находим два вектора, перпендикулярных нормали (базис плоскости)
    Vec3<float> up{0, 0, 1};
    if (std::abs(plane.normal.dot(up)) > 0.99f) up = {1, 0, 0};

    auto right = plane.normal.cross(up).normalize();
    up = right.cross(plane.normal).normalize();

    // 2. Создаем 4 вершины гигантского квадрата
    Face face;
    face.plane = plane;
    float size = 500.0f;
    Vec3<float> center = plane.normal * plane.distance;

    face.vertices.push_back(center + (right * size) + (up * size));
    face.vertices.push_back(center - (right * size) + (up * size));
    face.vertices.push_back(center - (right * size) - (up * size));
    face.vertices.push_back(center + (right * size) - (up * size));

    return face;
}
export void clipFace(Face& face, const Plane<float>& clipPlane) {
    if (face.vertices.empty()) return;
    std::vector<Vec3<float>> newVerts;

    for (size_t i = 0; i < face.vertices.size(); ++i) {
        const auto& p1 = face.vertices[i];
        const auto& p2 = face.vertices[(i + 1) % face.vertices.size()];

        float d1 = clipPlane.getDistance(p1);
        float d2 = clipPlane.getDistance(p2);

        // float d1 = clipPlane.getDistance(p1);
        // Сохраняем, если точка ВНУТРИ или НА плоскости
        if (d1 <= 0.001f) {
            newVerts.push_back(p1);
        }

        // Условие пересечения остается таким же (разные знаки)
        if ((d1 > 0.001f && d2 < -0.001f) || (d1 < -0.001f && d2 > 0.001f)) {
            float t = d1 / (d1 - d2);
            newVerts.push_back(p1 + (p2 - p1) * t);
        }
    }
    face.vertices = std::move(newVerts);
}

export struct RenderMesh {
    std::vector<float> vertices; // x, y, z
    void addFace(const Face& face) {
        if (face.vertices.size() < 3) return;
        Vec3<float> n = face.plane.normal; // Нормаль всей грани

        for (size_t i = 1; i < face.vertices.size() - 1; ++i) {
            // Добавляем 3 вершины, и у каждой будет одна и та же нормаль n
            auto addV = [&](Vec3<float> p) {
                vertices.push_back(p.x); vertices.push_back(p.y); vertices.push_back(p.z);
                vertices.push_back(n.x); vertices.push_back(n.y); vertices.push_back(n.z);

                float scale = 0.1f; // Плотность текстуры (1 метр = 0.1 текстуры)
                if (std::abs(n.y) > 0.5f) { // Пол/Потолок (проекция на XZ)
                    vertices.push_back(p.x * scale); vertices.push_back(p.z * scale);
                } else if (std::abs(n.x) > 0.5f) { // Стены влево/вправо (проекция на ZY)
                    vertices.push_back(p.z * scale); vertices.push_back(p.y * scale);
                } else { // Стены вперед/назад (проекция на XY)
                    vertices.push_back(p.x * scale); vertices.push_back(p.y * scale);
                }
            };
            addV(face.vertices[0]);
            addV(face.vertices[i]);
            addV(face.vertices[i+1]);
        }
    }
};
// Функция сборки браша
export RenderMesh generateVisualMesh(const std::vector<Plane<float>>& brushPlanes) {
    RenderMesh mesh;
    for (const auto& p : brushPlanes) {
        Face face = createHugePoly(p);
        for (const auto& clipper : brushPlanes) {
            if (&p == &clipper) continue;

            // Мы передаем саму плоскость.
            // ВАЖНО: внутри clipFace условие должно быть:
            // "сохраняем точку, если d <= 0" (точка за плоскостью, т.е. внутри куба)
            clipFace(face, clipper);
        }
        mesh.addFace(face);
    }
    return mesh;
}
export struct Vertex {
    Vec3<float> pos;
    Vec3<float> normal;
    float u, v;
    // Можно добавить Vec3<float> normal; для света позже
};

export struct GLMesh {
    GLuint vao, vbo;
    size_t vertexCount;

    void setup(const std::vector<Vertex>& vertices) {
        vertexCount = vertices.size();
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        // 0: Pos (offset 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        // 1: Normal (offset 12)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)12);
        glEnableVertexAttribArray(1);
        // 2: UV (offset 24)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)24);
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    void draw() const {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
    }
};
export struct Brush {
    std::vector<Plane<float>> planes;
    GLuint textureID;

    // Новые поля для сохранения
    Vec3<float> pos;
    Vec3<float> size;
};
export float rayIntersectAABB(Vec3<float> origin, Vec3<float> dir, Vec3<float> boxMin, Vec3<float> boxMax) {
    float t1 = (boxMin.x - origin.x) / dir.x;
    float t2 = (boxMax.x - origin.x) / dir.x;
    float t3 = (boxMin.y - origin.y) / dir.y;
    float t4 = (boxMax.y - origin.y) / dir.y;
    float t5 = (boxMin.z - origin.z) / dir.z;
    float t6 = (boxMax.z - origin.z) / dir.z;

    float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    if (tmax < 0 || tmin > tmax) return 1000000.0f; // Не попали
    return tmin; // Дистанция до точки входа
}

export struct Map {
    std::vector<Brush> brushes;
    GLMesh visualMesh;
    std::map<GLuint, GLMesh> textureGroups;
    void addBox(Vec3<float> center, Vec3<float> size, GLuint tex) {
        float x = size.x / 2.0f;
        float y = size.y / 2.0f;
        float z = size.z / 2.0f;

        // Создаем браш и сохраняем в него исходные pos и size
        Brush b;
        b.pos = center;
        b.size = size;
        b.textureID = tex;
        b.planes = {
            {{ 1, 0, 0}, center.x + x}, {{-1, 0, 0}, -(center.x - x)},
            {{ 0, 1, 0}, center.y + y}, {{ 0,-1, 0}, -(center.y - y)},
            {{ 0, 0, 1}, center.z + z}, {{ 0, 0,-1}, -(center.z - z)}
        };

        brushes.push_back(b);
    }
    // В класс Map
    void addWall(Vec3<float> start, Vec3<float> end, float thickness, float height, GLuint tex) {
        // Вычисляем центр и размер стены между двумя точками
        Vec3<float> center = (start + end) * 0.5f;
        center.y += height * 0.5f;
        float len = (end - start).length();

        // Для простоты — стены пока только вдоль осей X или Z
        Vec3<float> size = {
            std::abs(end.x - start.x) + thickness,
            height,
            std::abs(end.z - start.z) + thickness
        };
        addBox(center, size,tex);
    }

    // Лестница — это набор плоских брашей (ступенек)
    void addStairs(Vec3<float> start, Vec3<float> direction, int steps, float stepWidth, GLuint tex) {
        float sw = 1.0f; // глубина ступени
        float sh = 0.5f; // высота ступени
        for(int i = 0; i < steps; ++i) {
            Vec3<float> pos = start + (direction * (i * sw));
            pos.y += i * sh;
            addBox(pos, {stepWidth, sh, sw},tex);
        }
    }
    void build() {
        std::map<GLuint, std::vector<Vertex>> tempVerts;

        for (const auto& b : brushes) {
            RenderMesh bMesh = generateVisualMesh(b.planes);

            // Превращаем флоаты bMesh.vertices в структуру Vertex
            for (size_t i = 0; i + 7 < bMesh.vertices.size(); i += 8) {
                Vertex v;
                v.pos    = { bMesh.vertices[i],   bMesh.vertices[i+1], bMesh.vertices[i+2] };
                v.normal = { bMesh.vertices[i+3], bMesh.vertices[i+4], bMesh.vertices[i+5] };
                v.u = bMesh.vertices[i+6];
                v.v = bMesh.vertices[i+7];

                // Добавляем вершину именно в ту группу, которой принадлежит браш
                tempVerts[b.textureID].push_back(v);
            }
        }

        // 2. Создаем/обновляем GLMesh для каждой группы
        textureGroups.clear();
        for (auto& [texID, vertices] : tempVerts) {
            GLMesh mesh;
            mesh.setup(vertices);
            textureGroups[texID] = mesh;
        }
    }
    void save(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        for (const auto& b : brushes) {
            // Пишем в файл в формате: X Y Z | SX SY SZ | TEX
            file << b.pos.x << " " << b.pos.y << " " << b.pos.z << " | "
                 << b.size.x << " " << b.size.y << " " << b.size.z << " | "
                 << b.textureID << "\n";
        }
    }

    void load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return;

        brushes.clear();
        float x, y, z, sx, sy, sz;
        int tex;
        char sep;

        // Важно: читаем строго в том же порядке, что и сохраняли
        while (file >> x >> y >> z >> sep >> sx >> sy >> sz >> sep >> tex) {
            addBox({x, y, z}, {sx, sy, sz}, (GLuint)tex);
        }
        build();
    }
    void undo() {
        if (!brushes.empty()) {
            brushes.pop_back(); // Удаляем последний элемент из вектора
            build();            // Пересобираем меш, чтобы он исчез с экрана
            //std::cout << "Last brush removed!" << std::endl;
        }
    }
    int getBrushUnderCursor(Vec3<float> eyePos, Vec3<float> dir) {
        int hitIndex = -1;
        float minDistance = 100.0f; // Максимальная дальность "взгляда" (100 метров)

        for (int i = 0; i < brushes.size(); ++i) {
            // Вычисляем границы блока (AABB)
            Vec3<float> bMin = brushes[i].pos - (brushes[i].size * 0.5f);
            Vec3<float> bMax = brushes[i].pos + (brushes[i].size * 0.5f);

            float dist = rayIntersectAABB(eyePos, dir, bMin, bMax);
            if (dist < minDistance) {
                minDistance = dist;
                hitIndex = i;
            }
        }
        return hitIndex; // Вернет ID блока или -1
    }
};
