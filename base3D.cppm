// base3D.cppm
module; // Начало глобального фрагмента
// #define GLEW_STATIC
#include <GL/glew.h>
#include <SDL2/SDL.h> // Все инклуды ДОЛЖНЫ быть здесь
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <functional>
#include <map>
#include <string>
#include <memory>
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>
export module base3D;

export struct Options{
    std::string name="Test";

    int posx=SDL_WINDOWPOS_CENTERED,
    posy=SDL_WINDOWPOS_CENTERED,
    width=1290,
    height=720,
    index=-1;

    uint32_t window_flags=SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL,

    sdl_init=SDL_INIT_VIDEO|SDL_INIT_AUDIO,
    sdl_image_init=IMG_INIT_PNG;
};
// Теперь экспортируем нужные функции или структуры
export using SDL_Event = ::SDL_Event; // Экспортируем тип из глобального пространства

export using SDL_Window = ::SDL_Window;
export using SDL_GLContext = ::SDL_GLContext;
// Экспортируем функции (теперь они часть модуля)
export using ::SDL_SetRenderDrawColor;
export using ::SDL_CreateTexture;
export using ::SDL_CreateRGBSurface;
export using ::SDL_CreateTextureFromSurface;
export using ::SDL_GetKeyboardState;
export using ::SDL_SetRelativeMouseMode;
export using ::SDL_GetRelativeMouseState;
export using ::SDL_WarpMouseInWindow;
export using ::SDL_GetTicks;
export using ::SDL_GetTicks64;
export using ::SDL_GetPerformanceCounter;
// export using ::SDL_RenderClear;
// export using ::SDL_RenderPresent;
export using ::SDL_PollEvent;
// export using ::SDL_FillRect;
// export using ::SDL_RenderDrawRect;
// export using ::SDL_RenderFillRect;
export using ::SDL_MapRGB;
export using ::SDL_RenderCopy;
// export using ::SDL_HasIntersection;
export using ::SDL_GetMouseState;
export using ::SDL_GetWindowSize;
export using ::SDL_SetWindowSize;
export using ::SDL_SetWindowPosition;
// export using ::SDL_RenderSetVSync;
export using ::SDL_Delay;
// export using ::SDL_RenderDrawPoint;
export using ::SDL_GL_CreateContext;
export using ::SDL_GL_DeleteContext;
export using ::SDL_GL_SwapWindow;
export using ::SDL_GL_SetSwapInterval;
export using ::SDL_GL_SetAttribute;
export using ::SDL_Quit;

//TexturePtr texture{ SDL_CreateTextureFromSurface(app.render.get(), temp_surface) };
// Экспортируем типы, которые эти функции используют
// export using ::SDL_Renderer;
export using ::SDL_Window;
export using ::SDL_Texture;
export using ::SDL_Surface;
// export using ::SDL_Rect;
export using ::SDL_Color;
// export using ::SDL_Point;
export using ::Uint32;
export using ::IMG_Load;
export unsigned int CENTERED =0x2FFF0000u;

// Универсальный хелпер для SDL ресурсов
export template<typename T, auto Deleter>
using SDL_UniquePtr = std::unique_ptr<T, std::integral_constant<decltype(Deleter), Deleter>>;


// Теперь создание новых типов превращается в элегантный список:
export {
    using WindowPtr  = SDL_UniquePtr<SDL_Window,   SDL_DestroyWindow>;
    using GLcontextPtr = std::unique_ptr<void, std::integral_constant<decltype(&SDL_GL_DeleteContext), &SDL_GL_DeleteContext>>;
    using TexturePtr = SDL_UniquePtr<SDL_Texture,  SDL_DestroyTexture>;
    using SurfacePtr = SDL_UniquePtr<SDL_Surface,  SDL_FreeSurface>;
    using SoundPtr = SDL_UniquePtr<Mix_Chunk, Mix_FreeChunk>;
}

export struct StateApp{
    Options default_options;
    WindowPtr window;
    GLcontextPtr context;
    // RenderPtr render;
    bool run=false;
};

export struct SoundEfffect {
    std::string name;
    SoundPtr sound;
};

export struct TextureStorage {
    std::string name;
    // Rect rect;
};

export struct StorageTexture{
    std::map<std::string, TextureStorage> storage;
};

export struct StorageSoundEffect{
    std::map<std::string, SoundEfffect> storage;
};
export void sound_load(std::map<std::string, SoundEfffect>& storage,const std::string& key, const std::string& path) {
    Mix_Chunk* raw = Mix_LoadWAV(path.c_str());
    if (raw) {
        storage[key] = {key, SoundPtr(raw)};
    }
}

export void sound_play(std::map<std::string, SoundEfffect>& storage,const std::string& key) {
    if (storage.contains(key)) { // C++20 фича .contains()
        Mix_PlayChannel(-1, storage[key].sound.get(), 0);
    }
}

export class EventManager {
public:
    using Handler = std::function<void(const SDL_Event&)>;

    // Подписка на событие конкретного типа
    void subscribe(Uint32 eventType, Handler handler) {
        handlers[eventType].push_back(handler);
    }

    // Обработка всех накопившихся событий SDL
    void update() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (handlers.count(event.type)) {
                for (auto& handler : handlers[event.type]) {
                    handler(event);
                }
            }
        }
    }

private:
    std::map<Uint32, std::vector<Handler>> handlers;
};



// export struct Camera{
//     // Переменные состояния камеры
//     glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 0.0f);
//     glm::vec3 cameraFront = glm::vec3(0.0f, -1.0f, -0.5f);
//     glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

//     float yaw = -90.0f, pitch = -0.0f; // Углы поворота
//     float cameraSpeed = 1.0f; // Скорость полета
// };

// export glm::mat4 getView(Camera& camera){
//     return glm::lookAt(camera.cameraPos, camera.cameraPos + camera.cameraFront, camera.cameraUp);
// }

// export void cameraUpdate(Camera& camera){
//     const uint8_t* state = SDL_GetKeyboardState(NULL);
//     float speed = camera.cameraSpeed;

//     if (state[SDL_SCANCODE_W]) camera.cameraPos += speed * camera.cameraFront;
//     if (state[SDL_SCANCODE_S]) camera.cameraPos -= speed * camera.cameraFront;

//     glm::vec3 right = glm::normalize(glm::cross(camera.cameraFront, camera.cameraUp));
//     if (state[SDL_SCANCODE_A]) camera.cameraPos -= right * speed;
//     if (state[SDL_SCANCODE_D]) camera.cameraPos += right * speed;

//     // Вертикальное движение теперь по Y
//     if (state[SDL_SCANCODE_SPACE]) camera.cameraPos.y += speed;
//     if (state[SDL_SCANCODE_LSHIFT]) camera.cameraPos.y -= speed;
// }






export void initApp(StateApp &app){
    if (SDL_Init(app.default_options.sdl_init) < 0) return;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    if (IMG_Init(app.default_options.sdl_image_init)< 0) return;
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    // reset() сам удалит старое окно, если оно было
    app.window.reset(SDL_CreateWindow(
        app.default_options.name.data(),
        app.default_options.posx, app.default_options.posy,
        app.default_options.width, app.default_options.height,
        app.default_options.window_flags
    ));

    app.context.reset(SDL_GL_CreateContext(app.window.get()));

    // 2. Инициализация GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return;

    if (app.window) app.run = true;
    SDL_GL_SetSwapInterval(1);
}

export void closeApp(){
    Mix_CloseAudio();
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
}
