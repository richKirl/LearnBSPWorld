import std;
import MathGeometry;
import base3D;
import BSPTool;
import Shader;
import Animation;
import Shaders;
#include <GL/glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Camera {
    Vec3<float> pos{0.0f, 2.0f, 5.0f}; // Начальная позиция вне куба
    Vec3<float> cameraFront{0.0f, -1.0f, -0.5f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 0.05f; // Скорость полета


    Vec3<float> velocity{0, 0, 0}; // Вектор скорости
    float walkSpeed = 0.2f;   // Скорость разгона
    float gravity = -0.015f;  // Сила притяжения (каждый кадр)
    float jumpPower = 0.4f;   // Высота прыжка
    bool isGrounded = false;  // Стоим ли мы на чем-то
    bool isMoving=false;
    void update(){
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        Vec3<float> direction;
        direction.x = std::cos((yaw) * (PIPERGRAD)) * std::cos((pitch) * (PIPERGRAD));
        direction.y = std::sin((pitch) * (PIPERGRAD));
        direction.z = std::sin((yaw) * (PIPERGRAD)) * std::cos((pitch) * (PIPERGRAD));
        cameraFront = direction.normalize();
    }

    Mat4 getViewMatrix() const {
        return Mat4::lookAt(pos, pos + getForward(), {0, 1, 0});
    }
    // Вектор "Вперед" (уже был)
    Vec3<float> getForward() const {
        // Переводим градусы в радианы
        // float radYaw   = yaw   * (3.14159265f / 180.0f);
        // float radPitch = pitch * (3.14159265f / 180.0f);

        // Vec3<float> forward;
        // forward.x = std::cos(radYaw) * std::cos(radPitch);
        // forward.y = std::sin(radPitch);
        // forward.z = std::sin(radYaw) * std::cos(radPitch);

        return cameraFront;
    }

    Vec3<float> getRight() const {
        // Вектор "вправо" ВСЕГДА перпендикулярен мировому "вверх" (0, 1, 0)
        // Это не дает горизонту заваливаться (Camera Roll)
        return getForward().cross({0, 1, 0}).normalize();
    }
};


GLuint loadTexture(const char* path) {
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4); // Всегда 4 канала (RGBA)
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    // Настройки фильтрации Quake (Mipmaps обязательны!)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return id;
}

struct PointLight {
    Vec3<float> position;
    Vec3<float> color;
    float intensity;
    float range; // На какое расстояние светит
};
std::vector<PointLight> lights;
struct Platform {
    Vec3<float> pos;
    Vec3<float> size;
    float startY;

    void update(float time) {
        // Лифт ездит вверх-вниз на 10 единиц
        pos.y = startY + std::sin(time) * 10.0f;
    }
};


bool isColliding(Vec3<float> p, const Map& map) {
    float r = 0.4f; // Радиус игрока (ширина)
    float h = 1.8f; // Рост игрока

    for (const auto& b : map.brushes) {
        // Используй небольшой отступ, чтобы не застревать
        float epsilon = 0.01f;

        bool intersectX = std::abs(p.x - b.pos.x) < (r + b.size.x / 2.0f - epsilon);
        bool intersectY = (p.y < b.pos.y + b.size.y / 2.0f - epsilon) &&
                          (p.y + h > b.pos.y - b.size.y / 2.0f + epsilon);
        bool intersectZ = std::abs(p.z - b.pos.z) < (r + b.size.z / 2.0f - epsilon);


        if (intersectX && intersectY && intersectZ) return true;
    }
    return false;
}
struct Crosshair {
    GLuint vao, vbo;
    void setup() {
        float size = 0.02f;
        float thickness = 0.003f;

        // Корректируем горизонтальные размеры, деля их на аспект
        // Теперь 0.02 по X будет равно 0.02 по Y в пикселях
        float aspect = 1290.0f/720.0f;
        float sX = size / aspect;
        float tX = thickness / aspect;

        // Вертикальные оставляем как есть (база)
        float sY = size;
        float tY = thickness;

        std::vector<float> verts = {
            // Горизонтальная линия (теперь используем sX и tY)
            -sX,  tY, 0.0f,  sX,  tY, 0.0f,  sX, -tY, 0.0f,
            -sX,  tY, 0.0f,  sX, -tY, 0.0f, -sX, -tY, 0.0f,

            // Вертикальная линия (теперь используем tX и sY)
            -tX,  sY, 0.0f,  tX,  sY, 0.0f,  tX, -sY, 0.0f,
            -tX,  sY, 0.0f,  tX, -sY, 0.0f, -tX, -sY, 0.0f
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    void draw() {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 12); // Рисуем 4 треугольника (2 линии)
    }
};
// Создаем отдельный меш для пушки (один раз при старте)
// struct LocsObject{
//     GLint modelLoc;// = glGetUniformLocation(shader.ID, "model");
//     GLint viewLoc;// = glGetUniformLocation(shader.ID, "view");
//     GLint projLoc;// = glGetUniformLocation(shader.ID, "projection");
// };
GLMesh gunMesh;
void setupGun() {
    RenderMesh rMesh;
    // Вытянутый блок: x=0.1, y=0.1, z=0.8 (ствол)
    // Рисуем его относительно (0,0,0)
    rMesh = generateVisualMesh({
        {{ 1, 0, 0},  0.05f},
        {{-1, 0, 0},  0.05f},
        {{ 0, 1, 0},  0.05f},
        {{ 0,-1, 0},  0.05f},
        {{ 0, 0, 1},  0.4f},
        {{ 0, 0,-1},  0.4f}
    });

    // Конвертируем в Vertex (как мы делали для мира, i+=8)
    std::vector<Vertex> verts;
    for (size_t i = 0; i + 7 < rMesh.vertices.size(); i += 8) {
        Vertex v;
        // 1. Позиция (0, 1, 2)
        v.pos    = { rMesh.vertices[i],   rMesh.vertices[i+1], rMesh.vertices[i+2] };
        // 2. Нормаль (3, 4, 5)
        v.normal = { rMesh.vertices[i+3], rMesh.vertices[i+4], rMesh.vertices[i+5] };
        // 3. Текстурные координаты (6, 7)
        v.u = rMesh.vertices[i+6];
        v.v = rMesh.vertices[i+7];

        verts.push_back(v);
    }
    gunMesh.setup(verts);
}
void drawGun(Shader& shader,LocsObject& locsObject,const float aspect,const float time,const bool isMoving,const GLuint metalTex,const Mat4 proj,const float offsetgunz) {
    //glDisable(GL_DEPTH_TEST); // Чтобы ствол не врезался в стены
    glClear(GL_DEPTH_BUFFER_BIT);
    shader.use();

    // Эффект покачивания (синусоида)
    float bobX = isMoving ? sin(time * 10.0f) * 0.01f : 0;
    float bobY = isMoving ? cos(time * 15.0f) * 0.01f : sin(time * 2.0f) * 0.005f;

    // Матрица модели:
    // Сдвигаем вправо (0.4), вниз (-0.3) и чуть вперед (-0.5)
    Mat4 model;
   if(offsetgunz>0.0f)model = Mat4::translate(0.3f + bobX, -0.2f + bobY, -0.6f-offsetgunz) * Mat4::rotateY(110.0f); // Разворачиваем стволом от себя
   else model = Mat4::translate(0.4f + bobX, -0.3f + bobY, -0.6f) * Mat4::rotateY(180.0f);

    glUniformMatrix4fv(locsObject.modelLoc, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(locsObject.viewLoc, 1, GL_FALSE, Mat4::identity().m);
    glUniformMatrix4fv(locsObject.projLoc, 1, GL_FALSE, proj.m);
    glBindTexture(GL_TEXTURE_2D, metalTex);
    shader.setInt("ourTexture", 0);
    gunMesh.draw();

    //glEnable(GL_DEPTH_TEST);
}

enum class EnemyState { IDLE, CHASE, ATTACK, DEAD };

class Enemy {
public:
    std::string currentAnim = "enemyIdle";
    Mat4 scaleMatrix;
    // Vec3<float> scale;
    Vec3<float> position;
    Vec3<float> forward;
    //Vec3<float> up={0,1,0};
    float health = 100.0f;
    float animTime = 0.0f; // У каждого своя!
    EnemyState state = EnemyState::IDLE;


    Enemy(Vec3<float> startPos) : position(startPos) {
        animTime = 0.0f;
        // scale ={0.5f,0.5f,0.5f};
        scaleMatrix=Mat4::scale(0.2f,0.2f,0.2f);
    }

    void update(Vec3<float> playerPos, float dt) {
        if (state == EnemyState::DEAD) return;

        // 1. Используем векторную математику для направления
        Vec3<float> diff = playerPos - position;
        float distSq = diff.dot(diff); // Квадрат расстояния (быстрее, чем length)
        forward = (playerPos - position).normalize();
        forward.y = 0; // Зануляем Y, чтобы враг не наклонялся вверх/вниз
        forward = forward.normalize(); // Перенормализуем после правки Y

        if (distSq < 4.0f) { // 2.0^2
            state = EnemyState::ATTACK;
            currentAnim = "enemyAttack";

        }else if (distSq < 400.0f) { // 20.0^2
            state = EnemyState::CHASE;
            currentAnim = "enemyWalk";
            position = position + (this->forward * 3.0f * dt);
        }
        else {
            state = EnemyState::IDLE;
            currentAnim = "enemyIdle";
        }
    }

    void draw(AnimatedModel& model, Shader& shader, Mat4 proj, Mat4 view, float dt) {
        Mat4 offsetMatrix = Mat4::translate(0, model.offsetToBottom, 0);
        Mat4 modelMatrix = Mat4::translate(position.x,position.y,position.z)*Mat4::constructRotate(forward)*scaleMatrix*offsetMatrix;
        Vec3<float> color = {1.0f, 1.0f, 1.0f}; // Обычный цвет

        if (state == EnemyState::ATTACK) color = {1.0f, 0.5f, 0.5f}; // Краснеет при атаке

        model.drawAnim(currentAnim, animTime, dt, modelMatrix, proj, view, color, shader);
    }
};
std::vector<Enemy> enemies;

// Функция спавна
void spawnEnemy(Vec3<float> pos) {
    enemies.emplace_back(pos);
}
struct Particle {
    Vec3<float> pos;
    Vec3<float> vel;
    float life = 0.0f;
    float size = 0.2f;
};

struct Fireball {
    Vec3<float> position;
    Vec3<float> direction;
    float speed = 25.0f;
    float lifeTime = 3.0f;
    bool active = true;

    // Внутренний шлейф частиц для каждого фаербола
    std::vector<Particle> trail;
    int MAX_TRAIL = 30;

    Fireball(Vec3<float> pos, Vec3<float> dir) : position(pos), direction(dir) {
        trail.resize(MAX_TRAIL);
    }

    void update(float dt) {
        position = position + (direction * speed * dt);
        lifeTime -= dt;
        if (lifeTime <= 0) active = false;

        // Обновление частиц шлейфа
        for (auto& p : trail) {
            if (p.life > 0) {
                p.pos = p.pos + (p.vel * dt);
                p.life -= dt;
            } else {
                // Спавним новую частицу в хвосте
                p.pos = position;
                // Небольшой разброс назад
                p.vel = (direction * -5.0f) + Vec3<float>{(rand()%10-5)*0.5f, (rand()%10-5)*0.5f, (rand()%10-5)*0.5f};
                p.life = 0.3f;
            }
        }
    }
};

std::vector<Fireball> fireballs;

void shootFireball(Vec3<float> playerPos, Vec3<float> playerForward) {
    fireballs.emplace_back(playerPos + (playerForward * 1.5f), playerForward);
}

void updateFireballs(float dt, Enemy& enemy) {
    for (auto& f : fireballs) {
        if (!f.active) continue;
        f.update(dt);

        // Коллизия
        if ((f.position - enemy.position).length() < 1.2f) {
            enemy.health -= 50;
            f.active = false;
        }
    }
    fireballs.erase(std::remove_if(fireballs.begin(), fireballs.end(),
                   [](const Fireball& f) { return !f.active; }), fireballs.end());
}

// Функция отрисовки одного билборда (квадрата, развернутого к камере)
void drawBillboard(Shader& shader, Vec3<float> pos, float size, Vec3<float> camPos) {
    // Вычисляем матрицу модели так, чтобы она всегда смотрела на камеру (простой способ)
    Mat4 model = Mat4::translate(pos.x, pos.y, pos.z);

    // Обнуляем вращение в матрице вида или используем LookAt
    // Здесь мы просто передаем позицию, а в шейдере (см. предыдущее сообщение)
    // используем трюк с осями камеры. Если шейдер обычный — используем Scale:
    model = model * Mat4::scale(size, size, size);

    //shader.setMat4("model", model);
    // glDrawArrays(GL_TRIANGLES, 0, 6); // Рисуем квадрат
}

void drawFireballs(Shader& shader, Mat4 proj, Mat4 view, Vec3<float> camPos, unsigned int textureID) {
    shader.use();
    //shader.setMat4("projection", proj);
    //shader.setMat4("view", view);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Режим "Свечение"
    glDepthMask(GL_FALSE);             // Чтобы искры не перекрывали друг друга черными краями

    glBindTexture(GL_TEXTURE_2D, textureID);

    for (auto& f : fireballs) {
        // 1. Рисуем "Ядро"
        drawBillboard(shader, f.position, 0.8f, camPos);

        // 2. Рисуем "Искры" шлейфа
        for (auto& p : f.trail) {
            if (p.life > 0) {
                drawBillboard(shader, p.pos, p.size * (p.life / 0.3f), camPos);
            }
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

template<typename T>
void inputContoller(T& camera){
    const uint8_t* state = SDL_GetKeyboardState(NULL);
    //if (state[SDL_SCANCODE_ESCAPE]) app.run = false;
    // 1. Считаем желаемое направление движения (WASD)
    Vec3<float> wishDir{0, 0, 0};
    Vec3<float> forward = camera.getForward();
    forward.y = 1.0; // Ходим только по горизонтали
    forward = forward.normalize();
    Vec3<float> right = camera.getRight();
    camera.isMoving = false;
    if (state[SDL_SCANCODE_W]) {wishDir = wishDir + forward;camera.isMoving = true;}
    if (state[SDL_SCANCODE_S]) {wishDir = wishDir - forward;camera.isMoving = true;}
    if (state[SDL_SCANCODE_A]) {wishDir = wishDir - right;camera.isMoving = true;}
    if (state[SDL_SCANCODE_D]) {wishDir = wishDir + right;camera.isMoving = true;}
    // if (state[SDL_SCANCODE_N]) { // Клавиша N - Noclip
    //     camera.pos.y += 1.0f; // Просто подбросить вверх
    //     camera.velocity = {0,0,0};
    // }
    // 2. Применяем горизонтальную скорость и трение
    if (wishDir.length() > 0) {
        wishDir = wishDir.normalize() * camera.walkSpeed;
        camera.velocity.x = wishDir.x;
        camera.velocity.z = wishDir.z;
    } else {
        camera.velocity.x *= 0.5f; // Резкое торможение
        camera.velocity.z *= 0.5f;
    }

    // 3. Гравитация и Прыжок
    camera.velocity.y += camera.gravity;
    if (state[SDL_SCANCODE_SPACE] && camera.isGrounded) {
        camera.velocity.y = camera.jumpPower;
        camera.isGrounded = false;
        camera.isMoving=true;
    }
}

template<typename T>
void collederController(T& camera,const Map& world){
    // 4. ПРОВЕРКА КОЛЛИЗИЙ (Посекундное движение)
    // Пробуем сдвинуть по X
    Vec3<float> nextPos = camera.pos;
    nextPos.x += camera.velocity.x;
    if (!isColliding(nextPos, world)) camera.pos.x = nextPos.x;

    // Пробуем сдвинуть по Z
    nextPos = camera.pos;
    nextPos.z += camera.velocity.z;
    if (!isColliding(nextPos, world)) camera.pos.z = nextPos.z;

    // Пробуем сдвинуть по Y (Вертикаль)
    nextPos = camera.pos;
    nextPos.y += camera.velocity.y;

    if (!isColliding(nextPos, world)) {
        camera.pos.y = nextPos.y;
        camera.isGrounded = false;
    } else {
        // ВРЕЗАЛИСЬ
        if (camera.velocity.y < 0) { // Если летели ВНИЗ
            camera.isGrounded = true;
            // ВАЖНО: Выталкиваем игрока ПРЯМО на поверхность браша
            // Для этого нужно найти, на какой именно браш мы упали.
            // Но пока просто остановим падение:
            camera.velocity.y = 0;
        } else { // Если ударились головой об потолок
            camera.velocity.y = -0.01f; // Чуть-чуть оттолкнуть вниз
        }
    }
    //if(camera.isGrounded) camera.velocity.y=1.0f;
    // В игровом цикле (Update):
    for(auto& e : enemies) {
        e.update(camera.pos, 0.016f);
    }
}

int main(int argc, char* argv[]) {
    StateApp app;
    initApp(app); // Здесь должны инициализироваться SDL и OpenGL контекст
    // glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // 1. Подготовка данных (выносим функцию из main или делаем лямбдой)
    auto createCube = [](float x,float y,float z) -> std::vector<Plane<float>> {
        return {
            {{ 1, 0, 0},  x}, // x =  size
            {{-1, 0, 0},  x}, // x = -size
            {{ 0, 1, 0},  y}, // y =  size
            {{ 0,-1, 0},  y}, // y = -size
            {{ 0, 0, 1},  z}, // z =  size
            {{ 0, 0,-1},  z}  // z = -size
        };
    };
    Map world;
    float flash = 0.0f;
    // // 1. Центральный зал (Большая пустая коробка)
    // // Чтобы это была комната, мы делаем 6 тонких брашей (стены, пол, потолок)
    // world.addBox({0, -1, 0}, {50, 2, 50});   // Пол
    // world.addBox({0, 20, 0}, {50, 2, 50});   // Потолок
    // world.addBox({25, 10, 0}, {2, 20, 50});  // Правая стена
    // world.addBox({-25, 10, 0}, {2, 20, 50}); // Левая стена
    // world.addBox({0, 10, 25}, {50, 20, 2});  // Передняя стена
    // world.addBox({0, 10, -25}, {50, 20, 2}); // Задняя стена

    // // 2. Препятствия в центре (Колонны)
    // world.addBox({10, 5, 10}, {4, 10, 4});
    // world.addBox({-10, 5, -10}, {4, 10, 4});

    // // 3. Коридор (длинный узкий проход)
    // world.addBox({0, -1, 40}, {10, 2, 30});  // Пол коридора
    // world.addBox({5, 5, 40}, {1, 10, 30});   // Стена 1
    // world.addBox({-5, 5, 40}, {1, 10, 30});  // Стена 2
    GLuint wallTex = loadTexture("brick1.png");
    GLuint skyboxTex = loadTexture("sky1.png");
    GLuint metallTex = loadTexture("metall1.png");
    GLuint absact2Tex = loadTexture("abstract2.png");
    // 1. ПОЛ и ПОТОЛОК
    world.addBox({0, 0, 0}, {100, 1, 100},wallTex);
    world.addBox({0, 20, 0}, {100, 1, 100},wallTex);

    //колонны
    world.addBox({10, 5, 10}, {4, 10, 4},wallTex);
    world.addBox({-10, 5, -10}, {4, 10, 4},wallTex);
    world.addBox({10, 5, -10}, {4, 10, 4},wallTex);
    world.addBox({-10, 5, 10}, {4, 10, 4},wallTex);
    //платформа которую держат колонны
    world.addBox({0, 10, 0}, {30, 1, 30},wallTex);
    //переход на ярус
    world.addBox({-23, 10, 0}, {16, 1, 13},wallTex);
    //лестница для платформы со стороны входа
    world.addStairs({0, 0, 34}, {0, 0, -1}, 20, 20.0f, wallTex);
    // 2. СТЕНЫ С ПРОХОДАМИ (Задняя стена с дверью в центре)
    // Левая часть стены
    world.addBox({-30, 10, 50}, {40, 20, 2},wallTex);
    // Правая часть стены
    world.addBox({30, 10, 50}, {40, 20, 2},wallTex);

    // Перемычка над дверью (дверь высотой 8, шириной 20)
    world.addBox({0, 14, 50}, {20, 12, 2},wallTex);

    world.addBox({0, 0, 64}, {30, 1, 30},wallTex);  // Пол коридора
    world.addBox({10, 5, 64}, {1, 10, 30},wallTex);   // Стена 1
    world.addBox({-10, 5, 64}, {1, 10, 30},wallTex);  // Стена 2

    // 3. ВТОРОЙ ЯРУС (П-образный балкон)
    world.addBox({-40, 10, 0}, {20, 1, 100},wallTex); // Левый балкон
    world.addBox({40, 10, 0}, {20, 1, 100},wallTex);  // Правый балкон
    // ПРОДОЛЖЕНИЕ СЕВЕРНОГО КОРИДОРА (Удлиняем и поворачиваем направо)
    world.addBox({0, 0, 100}, {20, 1, 42}, wallTex);   // Пол дальше
    lights.push_back({{0, 6, 110}, {0.2f, 0.4f, 1.0f}, 15.0f, 35.0f});
    world.addBox({10, 5, 94}, {1, 10, 30}, wallTex);  // Правая стена
    world.addBox({-10, 5, 94}, {1, 10, 30}, wallTex); // Левая стена

    // ПОВОРОТ: Создаем Т-образный перекресток или Г-образный поворот
    world.addBox({30, 0, 119}, {40, 1, 20}, wallTex);  // Пол новой галереи справа
    world.addBox({30, 10, 109}, {40, 1, 20}, wallTex); // Потолок галереи
    world.addBox({30, 5, 119}, {40, 10, 1}, wallTex);  // Дальняя стена тупика
    // Основная белая лампа в центре хаба под потолком (высота 18)
    lights.push_back({{0, 18, 0}, {1.0f, 1.0f, 0.9f}, 25.0f, 60.0f});

    // 2. ЯРУС 2 (Балконы) - расставим по углам
    lights.push_back({{ 35, 12,  35}, {0.8f, 0.8f, 1.0f}, 12.0f, 30.0f}); // Синеватый угол
    lights.push_back({{-35, 12,  35}, {0.8f, 0.8f, 1.0f}, 12.0f, 30.0f});
    lights.push_back({{ 35, 12, -35}, {0.8f, 0.8f, 1.0f}, 12.0f, 30.0f});
    lights.push_back({{-35, 12, -35}, {0.8f, 0.8f, 1.0f}, 12.0f, 30.0f});

    // 3. КОРИДОРЫ (Входы)
    // Северный проход (Синий неон)
    lights.push_back({{0, 6, 45}, {0.2f, 0.4f, 1.0f}, 15.0f, 35.0f});
    // Южный проход (Зеленоватый)
    lights.push_back({{0, 6, -45}, {0.2f, 1.0f, 0.4f}, 15.0f, 35.0f});
    // Восточный и Западный (Теплый свет)
    lights.push_back({{ 45, 6, 0}, {1.0f, 0.6f, 0.2f}, 15.0f, 35.0f});
    lights.push_back({{-45, 6, 0}, {1.0f, 0.6f, 0.2f}, 15.0f, 35.0f});

    world.build();
    // Генерируем меш один раз перед циклом
    auto brush = createCube(500.0f,500.0f,500.0f);
    RenderMesh rMesh = generateVisualMesh(brush);

    // // 2. Превращаем "сырые" float в структуру Vertex
    std::vector<Vertex> glVertices;
    // Проверь это условие i + 5 и шаг i += 6
    for (size_t i = 0; i + 7 < rMesh.vertices.size(); i += 8) {
        Vertex v;
        // 1. Позиция (0, 1, 2)
        v.pos    = { rMesh.vertices[i],   rMesh.vertices[i+1], rMesh.vertices[i+2] };
        // 2. Нормаль (3, 4, 5)
        v.normal = { rMesh.vertices[i+3], rMesh.vertices[i+4], rMesh.vertices[i+5] };
        // 3. Текстурные координаты (6, 7)
        v.u = rMesh.vertices[i+6];
        v.v = rMesh.vertices[i+7];

        glVertices.push_back(v);
    }

    GLMesh cubeMesh;
    cubeMesh.setup(glVertices);
    // Загрузка шейдеров (пути к твоим файлам)
    Shader shader(vShader,fShader);
    Shader animshader(AVShader,AFShader);
    AnimatedModel animModel;

    Camera camera;
    camera.update();
    Crosshair crosshair;
    crosshair.setup();
    int w,h;
    SDL_GetWindowSize(app.window.get(), &w,&h);
    // Скрываем курсор для FPS-режима
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_WarpMouseInWindow(app.window.get(), w/2.0f,h/2.0f);
    bool toogleMouse = true;
    EventManager eventManagerGame;

    eventManagerGame.subscribe(SDL_QUIT, [&](const SDL_Event&) {
        app.run = false;
    });
    bool toogleWireFrame = false;
    eventManagerGame.subscribe(SDL_KEYDOWN, [&](const SDL_Event& e) {
        switch(e.key.keysym.sym){
            case SDLK_ESCAPE:{
                if(toogleMouse){
                    toogleMouse = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                else if(!toogleMouse){
                    app.run=false;
                }

                break;
            }
            case SDLK_F5:{
                //std::println("{}push lvl.txt",1);
                world.save("lvl.txt");
                break;
            }
            case SDLK_F9:{
                world.load("lvl.txt");
                break;
            }
            case SDLK_e:{
                // Ставим куб 2x2x2 в 5 метрах перед камерой
                Vec3<float> spawnPos = camera.pos + (camera.cameraFront * 5.0f);
                // Привязка к сетке (Grid Snapping)
                spawnPos.x = std::round(spawnPos.x / 2.0f) * 2.0f;
                spawnPos.y = (std::round(spawnPos.y / 2.0f) * 2.0f)+1.4f;
                spawnPos.z = std::round(spawnPos.z / 2.0f) * 2.0f;

                world.addBox(spawnPos, {2, 2, 2}, 1);
                world.build(); // Пересобираем меш, чтобы увидеть новый куб
                break;
            }
            case SDLK_TAB:{
                toogleWireFrame=!toogleWireFrame;
                if(toogleWireFrame)
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                break;
            }
            case SDLK_z:{
                world.undo();
                break;
            }
        default:{break;}
        }
    });
    eventManagerGame.subscribe(SDL_MOUSEBUTTONDOWN, [&](const SDL_Event& e) {
        if(!toogleMouse){
            toogleMouse=true;
            SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_WarpMouseInWindow(app.window.get(), w/2,h/2);
        }
    });
    // Обработка поворота мыши
    bool init=false;
    eventManagerGame.subscribe(SDL_MOUSEMOTION, [&](const SDL_Event& e) {
        if(toogleMouse){
            float sensitivity = 0.1f;
            camera.yaw   += e.motion.xrel * sensitivity;
            camera.pitch -= e.motion.yrel * sensitivity; // Инвертируем Y

            camera.update();
        }
    });

    float gunOffsetZ = 0.0f;
    setupGun();

    GLint modelLoc = glGetUniformLocation(shader.ID, "model");
    GLint viewLoc = glGetUniformLocation(shader.ID, "view");
    GLint projLoc = glGetUniformLocation(shader.ID, "projection");
    GLint viewposLoc = glGetUniformLocation(shader.ID, "viewPos");
    GLint fogDensityLoc = glGetUniformLocation(shader.ID, "fogDensity");
    GLint fogColorLoc = glGetUniformLocation(shader.ID, "fogColor");

    LocsObject locsObject;

    locsObject.modelLoc=modelLoc;
    locsObject.viewLoc=viewLoc;
    locsObject.projLoc=projLoc;
    LocsObject animLocs;
    animLocs.modelLoc = glGetUniformLocation(animshader.ID, "model");
    animLocs.viewLoc = glGetUniformLocation(animshader.ID, "view");
    animLocs.projLoc = glGetUniformLocation(animshader.ID, "projection");
    Mat4 modelM=Mat4::translate(0.0f, 0.5f, -10.0f);
    Vec3<float> col={0.3f, 2.0f,0.3f};
    float dtime;
    std::vector<std::string> pp={
        {"enemyIdle.bin"},
        {"enemyWalk.bin"},
        {"enemyRun.bin"},
        {"enemyAttack.bin"},
        };
    animModel.loadMultiple(pp,animLocs);
    // animModel.load("walk.bin",animLocs);
    //animModel.stat();
    // В инициализации:
    spawnEnemy({20.0f, 0.5f, -20.0f});
    spawnEnemy({-20.0f, 0.5f, -20.0f});
    // std::cout << glVertices.size() << std::endl;
    float lastFrame = 0.0f;
    while(app.run) {
    float currentFrame = SDL_GetPerformanceCounter(); // или аналог
    float deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
        eventManagerGame.update();
        uint32_t mouseState = SDL_GetRelativeMouseState(NULL, NULL);
        // 1. УДАЛЕНИЕ БЛОКА (Правая Кнопка Мыши)
        static bool rmbPressed = false;
        // if ((mouseState & (1 << ((3)-1))) && !rmbPressed) {
        //     // eyePos - это позиция камеры + eyeHeight (из прошлых шагов)
        //     int target = world.getBrushUnderCursor(camera.pos+1.8f, camera.cameraFront);
        //     if (target != -1) {
        //         world.brushes.erase(world.brushes.begin() + target);
        //         world.build(); // Перерисовываем мир без этого блока
        //         std::cout << "Brush deleted!" << std::endl;
        //     }
        //     rmbPressed = true;
        // } else if (!(mouseState & (1 << ((1)-1)))) {
        //     rmbPressed = false;
        // }
        float gunKick = 0.0f;
        if (!(mouseState & (1 << ((1)-1)))) {
            gunOffsetZ = 0.2f;
            flash = 1.0f;
            rmbPressed = false;
        }
        gunOffsetZ *= 0.85f;
        // Получаем состояние всех клавиш
        inputContoller(camera);

        collederController(camera,world);

        // Очистка экрана
        glClearColor(0.1f, 1.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float eyeHeight = 1.6f; // Твой рост
        Vec3<float> eyePos = camera.pos;
        eyePos.y += eyeHeight; // Поднимаем "глаза" над "ногами"

        // Теперь lookAt использует eyePos, а физика по-прежнему работает с camera.pos (ногами)
        Mat4 view = Mat4::lookAt(eyePos, eyePos + camera.cameraFront, {0, 1, 0});
        float fovRad = 45.0f * (PIPERGRAD);
        float aspect = 1290.0f/720.0f;
        Mat4 proj = Mat4::perspective(fovRad, aspect, 0.1f, 1000.0f);

        //Mat4 model =Mat4::identity();
        // Рендеринг
        shader.use();
        // Передаем позицию глаз (помним про eyeHeight!)
        //shader.setVec3("viewPos", camera.pos + Vec3<float>{0, 1.6f, 0});

        // Можно менять густоту тумана кнопками, если хочешь
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE,Mat4::identity().m);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);
        glUniform3f(viewposLoc,camera.pos.x +0,camera.pos.y +1.6f,camera.pos.z+ 0);
        glUniform1f(fogDensityLoc,0.02f);
        glUniform3f(fogColorLoc,0.05f, 0.05f, 0.1f);
        shader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, skyboxTex);
        shader.setInt("ourTexture", 0);
        cubeMesh.draw();
        shader.setInt("numLights", (int)lights.size());

        for (int i = 0; i < lights.size(); ++i) {
            std::string prefix = "lights[" + std::to_string(i) + "].";
            //GLint location, GLsizei count, GLboolean transpose, const GLfloat* value
            std::string pos =prefix + "position";
            std::string col =prefix + "color";
            glUniform3f(glGetUniformLocation(shader.ID,pos.c_str()),lights[i].position.x,lights[i].position.y,lights[i].position.z);
            glUniform3f(glGetUniformLocation(shader.ID,col.c_str()),lights[i].color.x,lights[i].color.y,lights[i].color.z);
            // shader.setVec3(prefix + "position", lights[i].position);
            // shader.setVec3(prefix + "color",    lights[i].color);
            shader.setFloat(prefix + "intensity", lights[i].intensity);
            shader.setFloat(prefix + "range",     lights[i].range);
        }
        // Проходим по всем группам текстур в мире
        for (auto& [texID, mesh] : world.textureGroups) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texID); // Биндим нужную текстуру (пол, стена и т.д.)
            shader.setInt("ourTexture", 0);

            mesh.draw(); // Рисуем только те куски карты, которые используют эту текстуру
        }

        //animModel.drawAnim("enemyAttack",dtime, 0.02f, modelM, proj, view, col, animshader);
        for(auto& e : enemies) {
            e.draw(animModel, animshader, proj, view,0.016f);
        }
        // animModel.draw(animshader,0.03f,modelM,proj,view,col);

        float time= SDL_GetTicks() / 1000.0f;

        drawGun(shader,locsObject,aspect,time,camera.isMoving,metallTex,proj,gunOffsetZ);
        //Mat4::translate(0, 0, gunKick);
        // gunKick *= 0.9f;
        // flash *= 0.9f;
        // if (flash < 0.01f) flash = 0.0f;
        // shader.setFloat("flashIntensity", flash);
        // glDisable(GL_DEPTH_TEST);

        // glEnable(GL_DEPTH_TEST);





        // glEnable(GL_DEPTH_TEST);
        // 1. Отключаем тест глубины, чтобы прицел был ПОВЕРХ всего
        glDisable(GL_DEPTH_TEST);
        // 2. Используем тот же шейдер, но с матрицей Identity (прямо на экран)
        shader.use();
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE,Mat4::identity().m);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, Mat4::identity().m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, Mat4::identity().m);

        crosshair.draw();
        glEnable(GL_DEPTH_TEST);

        SDL_GL_SwapWindow(app.window.get());
    }

    closeApp();
    return 0;
}
// if (state[SDL_SCANCODE_F5]) {
//     world.save("level.txt");
//     std::cout << "Level Saved!" << std::endl;
// }
// Клавиша F9 - Быстрая загрузка
// if (state[SDL_SCANCODE_F9]) {
//     world.load("level.txt");
//     std::cout << "Level Loaded!" << std::endl;
// }
// static bool ePressed = false;
// if (state[SDL_SCANCODE_E] && !ePressed) {
//     // Ставим куб 2x2x2 в 5 метрах перед камерой
//     Vec3<float> spawnPos = camera.pos + (camera.cameraFront * 5.0f);
//     // Привязка к сетке (Grid Snapping)
//     spawnPos.x = std::round(spawnPos.x / 2.0f) * 2.0f;
//     spawnPos.y = std::round(spawnPos.y / 2.0f) * 2.0f;
//     spawnPos.z = std::round(spawnPos.z / 2.0f) * 2.0f;

//     world.addBox(spawnPos, {2, 2, 2}, 1);
//     world.build(); // Пересобираем меш, чтобы увидеть новый куб
//     ePressed = true;
// } else if (!state[SDL_SCANCODE_E]) {
//     ePressed = false;
// }
// Пример: синяя лампа в коридоре
// lights.push_back({{0, 5, 40}, {0.2f, 0.5f, 1.0f}, 15.0f, 30.0f});
// // Яркая белая лампа в центре хаба
// lights.push_back({{0, 5, 0}, {1.0f, 1.0f, 0.8f}, 15.0f, 30.0f});
// // Пример: зулуная лампа в коридоре
// lights.push_back({{40, 5, 40}, {0.5f, 1.0f, 0.5f}, 15.0f, 30.0f});
// // Яркая зулуная лампа в центре хаба
// lights.push_back({{-40, 5, 40}, {0.5f, 1.0f, 0.5f}, 15.0f, 30.0f});
// // Пример: зулуная лампа в коридоре
// lights.push_back({{40, 5, -30}, {0.5f, 1.0f, 0.5f}, 15.0f, 30.0f});
// // Яркая зулуная лампа в центре хаба
// lights.push_back({{-40, 5, -30}, {0.5f, 1.0f, 0.5f}, 15.0f, 30.0f});
// // Яркая зулуная лампа в центре хаба
// lights.push_back({{0, 5, -40}, {0.5f, 0.2f, 1.0f}, 15.0f, 30.0f});
// // Пример: зулуная лампа в коридоре
// lights.push_back({{40, 5, 0}, {0.5f, 1.0f, 0.5f}, 15.0f, 30.0f});
// // Яркая зулуная лампа в центре хаба
// lights.push_back({{-40, 5, 0}, {0.5f, 1.0f, 0.5f}, 15.0f, 30.0f});
// 1. ЦЕНТРАЛЬНЫЙ ЗАЛ (Высокий свет)
// 4. ЛЕСТНИЦА (ведет на левый балкон)
//world.addStairs({-30, 0, -20}, {0, 0, 1}, 20, 10.0f, wallTex);
//world.addBox({0, 0, 0}, {500, 500, 500},skyboxTex);
// // В основном цикле:
// platform.update(SDL_GetTicks() * 0.001f);

// // "Хардкор" коллизия с лифтом:
// if (std::abs(camera.pos.x - platform.pos.x) < platform.size.x/2 &&
//     std::abs(camera.pos.z - platform.pos.z) < platform.size.z/2) {
//     if (camera.pos.y < platform.pos.y + 2.0f) { // 2.0f - рост игрока
//         camera.pos.y = platform.pos.y + 2.0f;
//         camera.velocity.y = 0;
//     }
// }
// bool checkCollision(Vec3<float> p, const Map& map) {
//     for (const auto& b : map.brushes) {
//         // Проверяем, попадает ли точка p внутрь коробки b
//         float r = 0.5f; // "Радиус" игрока
//         if (p.x + r > b.pos.x - b.size.x/2 && p.x - r < b.pos.x + b.size.x/2 &&
//             p.y + r > b.pos.y - b.size.y/2 && p.y - r < b.pos.y + b.size.y/2 &&
//             p.z + r > b.pos.z - b.size.z/2 && p.z - r < b.pos.z + b.size.z/2)
//         {
//             return true; // Столкновение!
//         }
//     }
//     return false;
// }
