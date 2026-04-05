module;
#include <GL/glew.h>
export module Animation;
export import std;
export import Shader;
export struct LocsObject{
    GLint modelLoc;// = glGetUniformLocation(shader.ID, "model");
    GLint viewLoc;// = glGetUniformLocation(shader.ID, "view");
    GLint projLoc;// = glGetUniformLocation(shader.ID, "projection");
};
export struct MyModelHeader {
    unsigned int vertCount;
    unsigned int frameCount;
};
// void drawEnemy(float deltaTime) {
//     static float animTime = 0.0f;
//     animTime += deltaTime * 10.0f; // Скорость анимации

//     int frameA = (int)animTime % header.frameCount;
//     int frameB = (frameA + 1) % header.frameCount;
//     float interpolation = animTime - (int)animTime;

//     // 1. Обновляем данные в GPU (только позиции)
//     glBindBuffer(GL_ARRAY_BUFFER, vboCurrent);
//     glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * 3 * sizeof(float), &allFrames[frameA * vertCount * 3]);

//     glBindBuffer(GL_ARRAY_BUFFER, vboNext);
//     glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * 3 * sizeof(float), &allFrames[frameB * vertCount * 3]);

//     // 2. Рендерим
//     shader.use();
//     shader.setFloat("interpolation", interpolation);
//     glBindVertexArray(vao);
//     glDrawArrays(GL_TRIANGLES, 0, vertCount);
// }
export std::string get_filename_only(std::string_view path) {
    // 1. Находим позицию последнего слеша
    size_t last_slash = path.find_last_of("/\\");

    // Оставляем только часть после слеша (имя файла с расширением)
    std::string_view filename = (last_slash == std::string_view::npos)
                                ? path
                                : path.substr(last_slash + 1);

    // 2. Находим первую точку в полученном имени файла
    size_t first_dot = filename.find_first_of('.');

    // Возвращаем часть до точки
    return std::string(filename.substr(0, first_dot));
}
export struct AnimationRange {
    int start;
    int end;
    float speed;
};

export std::map<std::string, AnimationRange> animations;
export class AnimatedModel {
private:
    MyModelHeader header;
    LocsObject locs;
    std::vector<float> allFrames; // Данные из файла
    GLuint vao, vboCurrent, vboNext;

    float animTime = 0.0f;

public:
    float offsetToBottom;
void stat(){
    std::println("{}",header.frameCount);
}
void loadMultiple(std::vector<std::string> filePaths, LocsObject locs1) {
    int currentGlobalFrame = 0;
    this->locs = locs1;
    for (const auto& path : filePaths) {
        std::ifstream file(path, std::ios::binary);
        MyModelHeader h;
        file.read((char*)&h, sizeof(h));

        // Запоминаем диапазон для этой анимации (например, "walk")
        std::string name = get_filename_only(path); // "idle", "walk" и т.д.
        //std::println("{}",name);
        animations[name] = { currentGlobalFrame, currentGlobalFrame + (int)h.frameCount - 1, 10.0f };

        // Дописываем кадры в общий список
        size_t dataSize = h.vertCount * h.frameCount * 6;
        size_t currentOffset = allFrames.size();
        allFrames.resize(currentOffset + dataSize);
        file.read((char*)&allFrames[currentOffset], dataSize * sizeof(float));

        currentGlobalFrame += h.frameCount;
        this->header.vertCount = h.vertCount; // Вершины везде одинаковые
    }
    this->header.frameCount = currentGlobalFrame; // Общее число кадров

    // Дальше стандартная настройка VAO/VBO...
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboCurrent);
    glGenBuffers(1, &vboNext);

    glBindVertexArray(vao);

    // --- Настройка vboCurrent (location 0 - Pos, location 2 - Normal) ---
    glBindBuffer(GL_ARRAY_BUFFER, vboCurrent);
    // Резервируем место под 6 float на вершину
    glBufferData(GL_ARRAY_BUFFER, header.vertCount * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Location 0: Позиция (первые 3 float)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // Location 2: Нормаль (следующие 3 float, смещение 3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    // --- Настройка vboNext (location 1 - Pos, location 3 - Normal) ---
    glBindBuffer(GL_ARRAY_BUFFER, vboNext);
    glBufferData(GL_ARRAY_BUFFER, header.vertCount * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Location 1: Позиция целевая
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // Location 3: Нормаль целевая
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));


    // Ищем самую низкую точку среди вершин первого кадра
    float minY = 1e10f;

    // Каждая вершина занимает 6 float (pos.x, pos.y, pos.z, norm.x, norm.y, norm.z)
    for (size_t i = 0; i < header.vertCount; ++i) {
        // Индекс Y-координаты для i-й вершины первого кадра:
        // i * 6 (пропускаем предыдущие вершины) + 1 (смещение до Y)
        float y = allFrames[i * 6 + 1];
        if (y < minY) {
            minY = y;
        }
    }

    // Теперь мы знаем, насколько нужно "поднять" модель, чтобы minY стал нулем
    this->offsetToBottom = -minY;

}
void load(const std::string& path, LocsObject locs1) {
    std::ifstream file(path, std::ios::binary);
    file.read((char*)&header, sizeof(header));

    // Важно: теперь данных в 2 раза больше (pos + normal)
    allFrames.resize(header.vertCount * header.frameCount * 6);
    file.read((char*)allFrames.data(), allFrames.size() * sizeof(float));

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboCurrent);
    glGenBuffers(1, &vboNext);

    glBindVertexArray(vao);

    // --- Настройка vboCurrent (location 0 - Pos, location 2 - Normal) ---
    glBindBuffer(GL_ARRAY_BUFFER, vboCurrent);
    // Резервируем место под 6 float на вершину
    glBufferData(GL_ARRAY_BUFFER, header.vertCount * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Location 0: Позиция (первые 3 float)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // Location 2: Нормаль (следующие 3 float, смещение 3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    // --- Настройка vboNext (location 1 - Pos, location 3 - Normal) ---
    glBindBuffer(GL_ARRAY_BUFFER, vboNext);
    glBufferData(GL_ARRAY_BUFFER, header.vertCount * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Location 1: Позиция целевая
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // Location 3: Нормаль целевая
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    this->locs = locs1;
}


    void draw(Shader& animShader, float dt, Mat4 modelMatrix, Mat4 proj, Mat4 view,Vec3<float>& color) {
        animTime += dt * 10.0f; // Скорость анимации

        int frameA = (int)animTime % header.frameCount;
        int frameB = (frameA + 1) % header.frameCount;
        float interpolation = animTime - (int)animTime;
        // float interpolation = 0.0f;
        // Заливаем данные текущих кадров в GPU
        glBindBuffer(GL_ARRAY_BUFFER, vboCurrent);
        glBufferSubData(GL_ARRAY_BUFFER, 0, header.vertCount * 6 * sizeof(float), &allFrames[frameA * header.vertCount * 6]);

        glBindBuffer(GL_ARRAY_BUFFER, vboNext);
        glBufferSubData(GL_ARRAY_BUFFER, 0, header.vertCount * 6 * sizeof(float), &allFrames[frameB * header.vertCount * 6]);
        animShader.use();
        glUniform3f(glGetUniformLocation(animShader.ID, "objectColor"),color.x,color.y,color.z);
        //animShader.setVec3("objectColor", color);
        animShader.setFloat("interpolation", interpolation);
        glUniformMatrix4fv(locs.modelLoc, 1, GL_FALSE,modelMatrix.m);
        glUniformMatrix4fv(locs.viewLoc, 1, GL_FALSE,view.m);
        glUniformMatrix4fv(locs.projLoc, 1, GL_FALSE,proj.m);
        // Подгружаем кадры в VBO
        glBindBuffer(GL_ARRAY_BUFFER, vboCurrent);
        glBufferSubData(GL_ARRAY_BUFFER, 0, header.vertCount * 6 * sizeof(float), &allFrames[frameA * header.vertCount * 6]);

        glBindBuffer(GL_ARRAY_BUFFER, vboNext);
        glBufferSubData(GL_ARRAY_BUFFER, 0, header.vertCount * 6 * sizeof(float), &allFrames[frameB * header.vertCount * 6]);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, header.vertCount);
    }
    void drawAnim(const std::string& animName, float& localTime, float dt, Mat4 modelMatrix, Mat4 proj, Mat4 view, Vec3<float>& color, Shader& animShader) {
        if (animations.find(animName) == animations.end()) return;
        AnimationRange range = animations[animName];

        // 1. Прибавляем время с учетом скорости клипа
        localTime += dt * range.speed;
        //int frameA = (int)animTime % header.frameCount;
        //int frameB = (frameA + 1) % header.frameCount;
        // 2. ГЛАВНЫЙ ФИКС: Если время вышло за конец клипа, возвращаем в начало клипа
        if (localTime >= (float)range.end) {
            localTime = (float)range.start;
        }
        // Если время меньше начала (на всякий случай)
        if (localTime < (float)range.start) {
            localTime = (float)range.start;
        }

        int frameA = (int)localTime;
        int frameB = frameA + 1;

        // 3. Зацикливаем интерполяцию: если B вышел за край, берем начало этого же клипа
        if (frameB > range.end) {
            frameB = range.start;
        }

        float interpolation = localTime - (int)localTime;

        glBindBuffer(GL_ARRAY_BUFFER, vboCurrent);
        glBufferSubData(GL_ARRAY_BUFFER, 0, header.vertCount * 6 * sizeof(float), &allFrames[frameA * header.vertCount * 6]);

        glBindBuffer(GL_ARRAY_BUFFER, vboNext);
        glBufferSubData(GL_ARRAY_BUFFER, 0, header.vertCount * 6 * sizeof(float), &allFrames[frameB * header.vertCount * 6]);
        animShader.use();
        glUniform3f(glGetUniformLocation(animShader.ID, "objectColor"),color.x,color.y,color.z);
        //animShader.setVec3("objectColor", color);
        animShader.setFloat("interpolation", interpolation);
        glUniformMatrix4fv(locs.modelLoc, 1, GL_FALSE,modelMatrix.m);
        glUniformMatrix4fv(locs.viewLoc, 1, GL_FALSE,view.m);
        glUniformMatrix4fv(locs.projLoc, 1, GL_FALSE,proj.m);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, header.vertCount);
        //std::println("Anim: {} | Time: {} | Range: {} - {}", animName, localTime, range.start, range.end);

    }

};
