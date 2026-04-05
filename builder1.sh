#!/bin/bash

# Настройки те же
CXX="clang++-20"
STD="-std=c++23"
STDLIB="-stdlib=libc++ -Wno-unused-command-line-argument"
LIBS="-lSDL2 -lSDL2_image -lSDL2_mixer -lGL -lGLEW"
MOD_EXT="cppm"
STD_MODULE_PATH="/usr/lib/llvm-20/share/libc++/v1/std.cppm"

# 1. База (std.pcm)
if [ ! -f "std.pcm" ]; then
    echo "--- Сборка стандартного модуля ---"
    $CXX $STD $STDLIB --precompile -o std.pcm "$STD_MODULE_PATH"
fi


# 2. Генерируем build.ninja
cat <<EOF > build.ninja
rule pcm
  command = $CXX $STD $STDLIB -fmodule-file=std=std.pcm  -fprebuilt-module-path=. --precompile \$in -o \$out
  description = Precompiling module \$out

rule obj
  command = $CXX $STD $STDLIB -fmodule-file=std=std.pcm -fprebuilt-module-path=. -c \$in -o \$out
  description = Compiling object \$out

rule link
  command = $CXX $STD $STDLIB -fmodule-file=std=std.pcm -fprebuilt-module-path=. \$in $LIBS -o \$out
  description = Linking app
EOF

# 3. Находим все модули и прописываем их в ninja
ALL_OBJS=""
ALL_PCMS=""
for f in *.$MOD_EXT; do
    name=$(basename "$f" .$MOD_EXT)
    echo "build $name.pcm: pcm $f | std.pcm" >> build.ninja
    echo "build $name.o: obj $name.pcm" >> build.ninja
    ALL_OBJS="$ALL_OBJS $name.o"
    ALL_PCMS="$ALL_PCMS $name.pcm" # Накапливаем список имен pcm
done

# 4. Теперь main.o железно знает, каких pcm ждать
echo "build main.o: obj main.cpp | $ALL_PCMS" >> build.ninja

# 5. Финальный таргет (линкуем всё вместе)
echo "build app: link main.o $ALL_OBJS" >> build.ninja

# 5. Запуск Ninja и программы
ninja
if [ $? -eq 0 ]; then
    echo "--- Запуск ---"
    ./app
fi
