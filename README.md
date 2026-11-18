# Домашнее задание №2: Модуль "ПОС" (C++). Шиленко Александр

## Архитектура
Выбрана **Clean Architecture / Ports and Adapters**.
Зависимости направлены строго внутрь:
1. `Domain` - Сущности (`Student`, `Criterion`, `Evaluation`), VO (`FinalGrade`), Domain Service (`IGradeCalculationStrategy`), Фабрика (`EvaluationFactory`).
2. `Application` - Use Cases (`POSApplication`), Порты (`IStudentRepository` и др.).
3. `Infrastructure` - Адаптеры (`InMemoryStudentRepo`, `InMemoryRubricRepo`, `InMemoryEvalRepo`), UI (`ConsoleFacade`).

## DDD (Domain-Driven Design)
* **Ubiquitous Language:** В коде используются термины предметной области (Студент, Критерий, Оценка, Итог).
* **Entity и VO:**
  * *Entity:* `Student`, `Criterion`.
  * *Value Object:* `FinalGrade`.
* **Инварианты:** Защищены в `EvaluationFactory` (score <= max_score) и `POSApplication` (нельзя оценить несуществующего студента/критерий).
* **Repository (Порт):** Интерфейсы для хранения данных абстрагированы от реализации (in-memory).
* **Domain Service:** `WeightedGradeStrategy` (расчет итога, так как зависит от множества сущностей).

## Паттерны GoF
1. **Порождающий: Factory Method** (`EvaluationFactory`). Решает проблему создания оценок с гарантией соблюдения инвариантов (score <= max_score).
2. **Структурный: Facade** (`ConsoleFacade`). Изолирует консольный интерфейс от сложной логики приложения и ввода-вывода.
3. **Поведенческий: Strategy** (`IGradeCalculationStrategy`). Позволяет легко менять алгоритм подсчета итогового балла без изменения доменной модели.

## Запуск и сборка
```bash
g++ -std=c++17 pos_app.cpp -o pos_app
./pos_app
```
