# BallisticCore — независимое ядро C++14

`src/core/core.pro` собирает статическую библиотеку без Qt (`CONFIG -= qt`). Публичные структуры и функции — в `src/core/model.h`, основной вход — `Solver::run` в `src/core/solver.h`. Библиотека не создаёт потоки, не обращается к UI, файлам или глобальному изменяемому состоянию. Приложение выбирает, в каком потоке вызвать расчёт. Пути ниже указаны относительно корня репозитория.

## Сборка

Обычный путь — открыть корневой `BallisticCalculator.pro`: он обеспечивает сборку ядра до приложения и тестов. Скрипт `build-release.cmd` помещает Release-библиотеку в `artifacts/qt-release/native/src/core/release/BallisticCore.lib`. В готовом приложении `dist/Release/BallisticCalculator.exe` ядро связано статически; отдельная DLL ядра для запуска не требуется.

Для самостоятельной сборки из отдельного каталога, в x64 Native Tools Command Prompt for VS 2017:

```bat
C:\Qt\Qt5.11.1\5.11.1\msvc2017_64\bin\qmake.exe <repository>\src\core\core.pro CONFIG+=release CONFIG-=debug
nmake
```

Результат: `release/BallisticCore.lib`. qmake используется только как генератор проекта; Qt DLL не нужны потребителю ядра. Для Debug: `CONFIG+=debug CONFIG-=release`, результат `debug/BallisticCored.lib`. Потребитель должен использовать совместимый компилятор, архитектуру и runtime: для проверенного MSVC 2017 x64 — `/MD` в Release, `/MDd` в Debug. Внешний ABI со стандартными контейнерами C++ требует согласованных настроек компиляции; бинарная совместимость с произвольными компиляторами не заявляется.

В проекте потребителя qmake достаточно задать абсолютный путь к каталогу сборки ядра и подключить:

```qmake
CONFIG += c++14
BALLISTIC_CORE_BUILD = C:/build/ballistic/core
include(<repository>/src/core/link.pri)
```

`link.pri` выбирает Release/Debug-библиотеку, добавляет заголовки и зависимость перелинковки. Сам по себе он не собирает ядро — порядок сборки задаёт общий проект или вызывающая система сборки.

## Пример без Qt

```cpp
#include "solver.h"
#include <iostream>

int main() {
    ballistic::Solver solver;
    ballistic::Parameters parameters;
    ballistic::Options options;
    const auto result = solver.run(parameters, options);
    if (result.status != ballistic::Status::Completed) {
        std::cerr << result.message << '\n';
        return 1;
    }
    const auto &last = result.trajectory.back();
    std::cout << last.radius - ballistic::EarthRadius << " m, "
              << last.velocity << " m/s\n";
}
```

Все расстояния внутри ядра — метры, масса — килограммы, время — секунды, тяга — ньютоны. Исключение для входного угла: `Parameters::turnDegrees` задан в градусах; углы `Sample` заданы в радианах. `mass` включает топливо; `exhaustVelocity` — удельный импульс в м/с (эффективная скорость истечения), соответствующий полю «Удельный импульс» в интерфейсе. Имя API и численные значения не изменены. В `Sample` хранится радиус от центра Земли; высота равна `radius - EarthRadius`.

`Options::optimize = false` выполняет один прогон с заданными входами. При `true` solver подбирает время и угол поворота по конечной высоте и модулю скорости. `Result::parameters` содержит точные параметры показанной траектории, `Result::options` — её настройки. Для воспроизведения результата передать эти параметры и настройки с `optimize = false`.

Отмена — третий аргумент `run`, функция `bool()`. Например, `[&] { return cancelled.load(); }`, где `cancelled` — `std::atomic<bool>`. Прогресс — четвёртый аргумент, функция `void(int iteration, double altitudeError, double velocityError)`. Обе функции вызываются синхронно из потока расчёта; они должны быстро возвращаться и не выбрасывать исключений. GUI обязан передавать обновления в свой поток. Неизменяемое ядро допускает независимые одновременные вызовы; захваченные callback-данные синхронизирует вызывающий код.

Статусы результата: `Completed`, `Cancelled`, `InvalidInput`, `NotConverged`, `NumericalFailure`. Успешную траекторию следует использовать только при `Completed`; при остальных статусах причину содержит строка `message` в UTF-8. Лимиты времени, шагов, прогонов и итераций задаются в `Options`.

Физические допущения, ограничения атмосферы и численная проверка приведены в [руководстве приложения](user-guide.ru.md). Лицензия ядра — GPL-3.0-or-later, см. корневые `LICENSE` и `NOTICE`.

Валидация: функции `validateVehicle`, `validateProgram` и `validateOptions` возвращают первую ошибку `ValidationIssue` (код, поле, индекс ступени с нуля, сообщение). `validateDetailed` объединяет эти проверки; прежняя `validate` сохраняет строковый интерфейс. Для программы дополнительно требуется корректная ракета. UI самостоятельно связывает `InputField` с полем формы.
