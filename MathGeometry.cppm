module;
#include <cmath>      // Импортируем стандартные библиотеки
#include <algorithm>
#include <concepts>
export module MathGeometry; // Объявляем имя модуля



export constexpr float PIPERGRAD =0.017453293f;
export template<typename T>
concept Numeric = std::floating_point<T> || std::integral<T>;

export template<Numeric T>
struct Vec3 {
    T x, y, z;

    // Сложение векторов
    auto operator+(const Vec3& v) const { return Vec3{x + v.x, y + v.y, z + v.z}; }
    auto operator+(const float v) const { return Vec3{x + v, y + v, z + v}; }
    auto operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    auto operator-(const Vec3& v) const { return Vec3{x - v.x, y - v.y, z - v.z}; }
    auto operator-(const float v) const { return Vec3{x - v, y - v, z - v}; }
    auto operator*(const Vec3& v) const { return Vec3{x * v.x, y * v.y, z * v.z}; }
    auto operator*(const float v) const { return Vec3{x * v, y * v, z * v}; }
    // Скалярное произведение (Dot Product) — КРИТИЧНО для BSP
    // Если результат > 0, точка перед плоскостью. Если < 0 — за ней.
    auto dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }

    // Векторное произведение (Cross Product) — для поиска нормалей
    auto cross(const Vec3& v) const {
        return Vec3{
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        };
    }

    auto length() const { return std::sqrt(dot(*this)); }

    auto normalize() const {
        T len = length();
        return len > 0 ? Vec3{x / len, y / len, z / len} : Vec3{0, 0, 0};
    }
    // Линейная интерполяция между векторами
    // t = 0.0 (старт), t = 1.0 (цель)
    static Vec3 lerp(const Vec3& start, const Vec3& end, float t) {
        // Формула: start + (end - start) * t
        return start + (end - start) * t;
    }

};
// Добавь это ВНЕ структуры Vec3 для поддержки 2.0f * vec
export template<Numeric T>
auto operator*(float s, const Vec3<T>& v) { return v * s; }

export float distance(Vec3<float> a, Vec3<float> b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}
export float lerpAngle(float current, float target, float t) {
    float diff = std::fmod(target - current + 180.0f, 360.0f) - 180.0f;
    if (diff < -180.0f) diff += 360.0f;
    return current + diff * std::clamp(t, 0.0f, 1.0f);
}
export float interpolateAngle(float current, float target, float speed, float dt) {
    // Находим кратчайшую разницу между углами (-180...180)
    float delta = target - current;
    while (delta > 180.0f)  delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;

    // Ограничиваем поворот за этот кадр
    float maxRotation = speed * dt;
    // std::clamp ограничит дельту, чтобы враг не проскочил цель
    return current + std::clamp(delta, -maxRotation, maxRotation);
}
export struct Mat4 {
    float m[16] = {0};
    Mat4 operator*(const Mat4& other) const {
        Mat4 res;
        for (int col = 0; col < 4; ++col) {       // По столбцам правой матрицы
            for (int row = 0; row < 4; ++row) {   // По строкам левой матрицы
                float sum = 0.0f;
                for (int i = 0; i < 4; ++i) {
                    // m[row + i*4] — элемент левой матрицы (строка row)
                    // other.m[i + col*4] — элемент правой матрицы (столбец col)
                    sum += m[row + i * 4] * other.m[i + col * 4];
                }
                res.m[row + col * 4] = sum;
            }
        }
        return res;
    }
    static Mat4 identity() {
        Mat4 res;
        res.m[0] = 1; res.m[5] = 1; res.m[10] = 1; res.m[15] = 1;
        return res;
    }

    static Mat4 perspective(float fovRad, float aspect, float nearP, float farP) {
        Mat4 res;
        float f = 1.0f / std::tan(fovRad / 2.0f);
        res.m[0] = f / aspect;
        res.m[5] = f;
        res.m[10] = (farP + nearP) / (nearP - farP);
        res.m[11] = -1.0f; // Это индекс 11 (строка 4, столбец 3) - ПРАВИЛЬНО
        res.m[14] = (2.0f * farP * nearP) / (nearP - farP); // Ошибка была тут: m[14], а не m[11]
        res.m[15] = 0.0f;
        return res;
    }

    static Mat4 lookAt(Vec3<float> eye, Vec3<float> center, Vec3<float> up) {
        Vec3 f = (center - eye).normalize();
        Vec3 s = f.cross(up).normalize(); // Right
        Vec3 u = s.cross(f);              // Up (ortho)

        Mat4 res = Mat4::identity();

        // 1-й СТОЛБЕЦ (Right)
        res.m[0] = s.x;
        res.m[1] = u.x;
        res.m[2] = -f.x;
        res.m[3] = 0.0f;

        // 2-й СТОЛБЕЦ (Up)
        res.m[4] = s.y;
        res.m[5] = u.y;
        res.m[6] = -f.y;
        res.m[7] = 0.0f;

        // 3-й СТОЛБЕЦ (Forward/Back)
        res.m[8] = s.z;
        res.m[9] = u.z;
        res.m[10] = -f.z;
        res.m[11] = 0.0f;

        // 4-й СТОЛБЕЦ (Translation)
        res.m[12] = -s.dot(eye);
        res.m[13] = -u.dot(eye);
        res.m[14] = f.dot(eye);
        res.m[15] = 1.0f;

        return res;
    }
    static Mat4 translate(float x, float y, float z) {
        Mat4 res = Mat4::identity();
        res.m[12] = x;
        res.m[13] = y;
        res.m[14] = z;
        return res;
    }
    static Mat4 translate(Vec3<float> v) {
        Mat4 res = Mat4::identity();
        res.m[12] = v.x;
        res.m[13] = v.y;
        res.m[14] = v.z;
        return res;
    }
    static Mat4 rotateX(float angleRad) {
        Mat4 res = Mat4::identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        res.m[5] = c;  res.m[6] = s;
        res.m[9] = -s; res.m[10] = c;
        return res;
    }

    static Mat4 rotateY(float angleRad) {
        Mat4 res = Mat4::identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        res.m[0] = c;  res.m[2] = -s;
        res.m[8] = s;  res.m[10] = c;
        return res;
    }

    static Mat4 rotateZ(float angleRad) {
        Mat4 res = Mat4::identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        res.m[0] = c;  res.m[1] = s;
        res.m[4] = -s; res.m[5] = c;
        return res;
    }
    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 res = Mat4::identity();
        res.m[0] = sx;  // Масштаб по X
        res.m[5] = sy;  // Масштаб по Y
        res.m[10] = sz; // Масштаб по Z
        return res;
    }
    static Mat4 constructRotate(Vec3<float> forward){
        Vec3<float> worldUp = {0.0f, 1.0f, 0.0f};
        Vec3<float> right = worldUp.cross(forward).normalize();

        // 3. Реальный "Верх" (Up) для врага
        Vec3<float> up = forward.cross(right);
        Mat4 rotationMatrix = Mat4::identity();
        // // 1-й СТОЛБЕЦ (Right)
        // res.m[0] = s.x;
        // res.m[1] = u.x;
        // res.m[2] = -f.x;
        // res.m[3] = 0.0f;

        // // 2-й СТОЛБЕЦ (Up)
        // res.m[4] = s.y;
        // res.m[5] = u.y;
        // res.m[6] = -f.y;
        // res.m[7] = 0.0f;

        // // 3-й СТОЛБЕЦ (Forward/Back)
        // res.m[8] = s.z;
        // res.m[9] = u.z;
        // res.m[10] = -f.z;
        // res.m[11] = 0.0f;
        // Первый столбец (Ось X)
        rotationMatrix.m[0] = right.x;
        rotationMatrix.m[1] = right.y;
        rotationMatrix.m[2] = right.z;

        // Второй столбец (Ось Y)
        rotationMatrix.m[4] = up.x;
        rotationMatrix.m[5] = up.y;
        rotationMatrix.m[6] = up.z;

        // Третий столбец (Ось Z)
        rotationMatrix.m[8] = forward.x;
        rotationMatrix.m[9] = forward.y;
        rotationMatrix.m[10] = forward.z;
        return rotationMatrix;
    }

};
