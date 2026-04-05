// Shader.cppm
module; // Начало глобального фрагмента


export module Shaders;

// --- ШЕЙДЕРЫ КАК СТРОКИ ---
export const char* vShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoords;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    out vec3 FragPos;
    out vec3 Normal; // Передаем нормаль в фрагментный шейдер
    out vec2 TexCoords;
    void main() {
        // В Quake 3 для статики нормали можно не трансформировать сложно,
        // но по фэншую лучше так:
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        TexCoords = aTexCoords;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

export const char* fShader = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;
    in vec2 TexCoords;
    out vec4 FragColor;

    struct Light {
        vec3 position;
        vec3 color;
        float intensity;
        float range;
    };

    uniform Light lights[16];
    uniform int numLights;
    uniform sampler2D ourTexture;

    // ПАРАМЕТРЫ ТУМАНА И КАМЕРЫ
    uniform vec3 viewPos; // ПОЗИЦИЯ ГЛАЗ (нужно передать из C++)
    uniform vec3 fogColor = vec3(0.05, 0.05, 0.1); // Темно-синий "квейковский" туман
    uniform float fogDensity = 0.02;
    uniform float flashIntensity;
    void main() {
        vec3 norm = normalize(Normal);
        vec3 ambient = vec3(0.1);
        vec3 lightAccum = vec3(0.0);

        // 1. РАСЧЕТ ОСВЕЩЕНИЯ (результат складываем в lightAccum)
        for(int i = 0; i < numLights; i++) {
            float dist = distance(lights[i].position, FragPos);
            if (dist < lights[i].range) {
                vec3 lightDir = normalize(lights[i].position - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * (dist * dist));
                lightAccum += (lights[i].color * diff * lights[i].intensity * attenuation);
            }
        }

        // 2. ТЕКСТУРИРОВАНИЕ И ИТОГОВЫЙ ЦВЕТ ОБЪЕКТА
        vec4 texColor = texture(ourTexture, TexCoords);
        vec3 flashColor = vec3(1.0, 0.8, 0.4) * flashIntensity;
        vec3 objectColor = (ambient + lightAccum) * (texColor.rgb);
        //vec3 totalLight = ambient + lightAccum + (vec3(1.0, 0.8, 0.4) * flashIntensity);

        // Умножаем текстуру на суммарный свет
        //vec3 objectColor = texColor.rgb * totalLight;

        // 3. РАСЧЕТ ТУМАНА
        float d = distance(viewPos, FragPos);
        // Экспоненциальный туман: чем дальше d, тем меньше fogFactor
        float fogFactor = exp(-pow(d * fogDensity, 2.0));
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        // Смешиваем цвет тумана и цвет объекта
        // Если fogFactor = 1 (близко), видим объект. Если 0 (далеко) — только туман.
        vec3 finalColor = mix(fogColor, objectColor, fogFactor);

        FragColor = vec4(finalColor, 1.0);
    }
)";
export const char* AVShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aTargetPos;
    layout (location = 2) in vec3 aNormal;
    layout (location = 3) in vec3 aTargetNormal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform float interpolation;

    out vec3 Normal;
    out vec3 FragPos;

    void main() {
        vec3 pos = mix(aPos, aTargetPos, interpolation);
        // Смешиваем нормали и ОБЯЗАТЕЛЬНО нормализуем после mix
        vec3 norm = normalize(mix(aNormal, aTargetNormal, interpolation));

        FragPos = vec3(model * vec4(pos, 1.0));
        Normal = mat3(transpose(inverse(model))) * norm;

        gl_Position = projection * view * vec4(FragPos, 1.0);

    }
)";
export const char* AFShader = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;
    out vec4 FragColor;

    uniform vec3 objectColor; // Наш цвет (например, красный для врага)
    uniform vec3 viewPos;
    uniform vec3 fogColor;
    uniform float fogDensity;

    void main() {
        // Пока просто заливаем цветом + добавляем твой туман
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
        float diff = max(dot(normalize(Normal), lightDir), 0.0);
        float lighting = 0.3 + (0.7 * diff);
        float d = distance(viewPos, FragPos);
        float fogFactor = exp(-pow(d * fogDensity, 2.0));

        vec3 result = mix(fogColor, objectColor, clamp(fogFactor, 0.0, 1.0));
        FragColor = vec4(result*diff, 1.0);
    }
)";
// // fShader
// uniform vec3 lightPositions[4]; // 4 лампы на комнату
// void main() {
//     float totalLight = 0.2; // Ambient
//     for(int i = 0; i < 4; i++) {
//         float d = distance(FragPos, lightPositions[i]);
//         totalLight += 1.0 / (d * d); // Затухание света по закону квадратов
//     }
//     FragColor = texture(ourTexture, TexCoords) * totalLight;
// }
// #version 330 core
// in vec3 Normal;
// in vec2 TexCoords; // Приходит из вертексного
// out vec4 FragColor;

// uniform sampler2D ourTexture;

// void main() {
//     vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
//     float diff = max(dot(normalize(Normal), lightDir), 0.0);

//     // 0.3 - это фоновый свет. Даже в полной тени стена будет видна на 30%
//     float lighting = 0.3 + (0.7 * diff);

//     vec4 texColor = texture(ourTexture, TexCoords);

//     // Если текстура не привязана, FragColor будет черным.
//     // Проверим: если текстура пустая, выведем хотя бы серый цвет
//     if(texColor.a < 0.1) texColor = vec4(0.5, 0.5, 0.5, 1.0);

//     FragColor = texColor * lighting;
// }
