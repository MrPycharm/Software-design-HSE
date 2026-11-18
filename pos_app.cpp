#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct Student
{
    std::string id;
    std::string name;
};

struct Criterion
{
    std::string id;
    std::string name;
    double max_score;
    double weight;
};

struct Evaluation
{
    std::string student_id;
    std::string criterion_id;
    double score;
};

struct FinalGrade
{
    std::string student_id;
    double total_score;
};

class IGradeCalculationStrategy
{
public:
    virtual ~IGradeCalculationStrategy() = default;
    virtual double calculate(
        const std::vector<Evaluation>& evals,
        const std::unordered_map<std::string, Criterion>& criteria) const = 0;
};

class WeightedGradeStrategy: public IGradeCalculationStrategy
{
public:
    double calculate(const std::vector<Evaluation>& evals,
                     const std::unordered_map<std::string, Criterion>& criteria)
        const override
    {
        double final_score = 0.0;
        for(const auto& ev: evals)
        {
            auto it = criteria.find(ev.criterion_id);
            if(it != criteria.end())
            {
                const auto& crit = it->second;
                double normalized = (ev.score / crit.max_score) * crit.weight;
                final_score += normalized;
            }
        }
        return std::round(final_score * 100.0) / 100.0;
    }
};

class EvaluationFactory
{
public:
    static Evaluation create(const std::string& student_id,
                             const Criterion& criterion, double score)
    {
        if(score < 0 || score > criterion.max_score)
        {
            throw std::invalid_argument(
                "Оценка " + std::to_string(score) +
                " выходит за рамки критерия '" + criterion.name +
                "' (макс: " + std::to_string(criterion.max_score) + ")");
        }
        return Evaluation{student_id, criterion.id, score};
    }
};

struct StudentReportDTO
{
    std::string student_id;
    std::string student_name;
    double final_score;
};

class IStudentRepository
{
public:
    virtual ~IStudentRepository() = default;
    virtual void save(const Student& student) = 0;
    virtual std::optional<Student> get(const std::string& id) const = 0;
    virtual std::vector<Student> getAll() const = 0;
};

class IRubricRepository
{
public:
    virtual ~IRubricRepository() = default;
    virtual void save(const Criterion& criterion) = 0;
    virtual std::optional<Criterion> get(const std::string& id) const = 0;
    virtual std::unordered_map<std::string, Criterion> getAll() const = 0;
};

class IEvaluationRepository
{
public:
    virtual ~IEvaluationRepository() = default;
    virtual void save(const Evaluation& eval) = 0;
    virtual std::vector<Evaluation>
    getByStudent(const std::string& student_id) const = 0;
    virtual void remove(const std::string& student_id,
                        const std::string& criterion_id) = 0;
};

class IGradeExporter
{
public:
    virtual ~IGradeExporter() = default;
    virtual void
    exportAllToFile(const std::string& filename,
                    const std::vector<StudentReportDTO>& reports) = 0;
};

class POSApplication
{
private:
    std::shared_ptr<IStudentRepository> studentRepo;
    std::shared_ptr<IRubricRepository> rubricRepo;
    std::shared_ptr<IEvaluationRepository> evalRepo;
    std::shared_ptr<IGradeCalculationStrategy> strategy;
    std::shared_ptr<IGradeExporter> exporter;

public:
    POSApplication(std::shared_ptr<IStudentRepository> sRepo,
                   std::shared_ptr<IRubricRepository> rRepo,
                   std::shared_ptr<IEvaluationRepository> eRepo,
                   std::shared_ptr<IGradeCalculationStrategy> strat,
                   std::shared_ptr<IGradeExporter> exp)
        : studentRepo(sRepo), rubricRepo(rRepo), evalRepo(eRepo),
          strategy(strat), exporter(exp)
    {
    }

    void addStudent(const std::string& id, const std::string& name)
    {
        studentRepo->save(Student{id, name});
    }

    void addCriterion(const std::string& id, const std::string& name,
                      double max_score, double weight)
    {
        rubricRepo->save(Criterion{id, name, max_score, weight});
    }

    void gradeStudent(const std::string& student_id,
                      const std::string& criterion_id, double score)
    {
        if(!studentRepo->get(student_id).has_value())
            throw std::invalid_argument("Студент с ID '" + student_id +
                                        "' не найден.");

        auto criterion = rubricRepo->get(criterion_id);
        if(!criterion.has_value())
            throw std::invalid_argument("Критерий с ID '" + criterion_id +
                                        "' не найден.");

        evalRepo->remove(student_id, criterion_id);
        Evaluation eval =
            EvaluationFactory::create(student_id, criterion.value(), score);
        evalRepo->save(eval);
    }

    void removeEvaluation(const std::string& student_id,
                          const std::string& criterion_id)
    {
        evalRepo->remove(student_id, criterion_id);
    }

    FinalGrade getStudentFinalGrade(const std::string& student_id)
    {
        auto evals = evalRepo->getByStudent(student_id);
        auto criteria = rubricRepo->getAll();
        double total = strategy->calculate(evals, criteria);
        return FinalGrade{student_id, total};
    }

    void exportAllGrades(const std::string& filename)
    {
        auto students = studentRepo->getAll();
        if(students.empty())
            throw std::runtime_error("Нет студентов для экспорта.");

        std::vector<StudentReportDTO> reports;
        for(const auto& student: students)
        {
            auto grade = getStudentFinalGrade(student.id);
            reports.push_back(
                StudentReportDTO{student.id, student.name, grade.total_score});
        }

        exporter->exportAllToFile(filename, reports);
    }

    std::optional<Student> getStudent(const std::string& id)
    {
        return studentRepo->get(id);
    }
    std::vector<Student> getAllStudents() { return studentRepo->getAll(); }
    std::unordered_map<std::string, Criterion> getAllCriteria()
    {
        return rubricRepo->getAll();
    }
    std::vector<Evaluation> getStudentEvals(const std::string& id)
    {
        return evalRepo->getByStudent(id);
    }
};

class InMemoryStudentRepo: public IStudentRepository
{
    std::unordered_map<std::string, Student> data;

public:
    void save(const Student& student) override { data[student.id] = student; }
    std::optional<Student> get(const std::string& id) const override
    {
        auto it = data.find(id);
        if(it != data.end())
            return it->second;
        return std::nullopt;
    }
    std::vector<Student> getAll() const override
    {
        std::vector<Student> res;
        for(const auto& [id, st]: data)
            res.push_back(st);
        return res;
    }
};

class InMemoryRubricRepo: public IRubricRepository
{
    std::unordered_map<std::string, Criterion> data;

public:
    void save(const Criterion& criterion) override
    {
        data[criterion.id] = criterion;
    }
    std::optional<Criterion> get(const std::string& id) const override
    {
        auto it = data.find(id);
        if(it != data.end())
            return it->second;
        return std::nullopt;
    }
    std::unordered_map<std::string, Criterion> getAll() const override
    {
        return data;
    }
};

class InMemoryEvalRepo: public IEvaluationRepository
{
    std::vector<Evaluation> data;

public:
    void save(const Evaluation& eval) override { data.push_back(eval); }
    std::vector<Evaluation>
    getByStudent(const std::string& student_id) const override
    {
        std::vector<Evaluation> result;
        for(const auto& e: data)
            if(e.student_id == student_id)
                result.push_back(e);
        return result;
    }
    void remove(const std::string& student_id,
                const std::string& criterion_id) override
    {
        data.erase(std::remove_if(data.begin(), data.end(),
                                  [&](const Evaluation& e)
                                  {
                                      return e.student_id == student_id &&
                                             e.criterion_id == criterion_id;
                                  }),
                   data.end());
    }
};

class TxtGradeExporter: public IGradeExporter
{
public:
    void exportAllToFile(const std::string& filename,
                         const std::vector<StudentReportDTO>& reports) override
    {
        std::ofstream out(filename);
        if(!out.is_open())
            throw std::runtime_error("Не удалось открыть файл для записи.");

        out << "ID Студента,Имя,Итоговый балл\n";
        for(const auto& rep: reports)
        {
            out << rep.student_id << "," << rep.student_name << ","
                << rep.final_score << "\n";
        }
        out.close();
    }
};

class ConsoleFacade
{
private:
    std::shared_ptr<POSApplication> app;

    void clearInput()
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    double readDouble(const std::string& prompt)
    {
        double val;
        while(true)
        {
            std::cout << prompt;
            if(std::cin >> val)
                return val;
            std::cout << "Ошибка: введите число.\n";
            clearInput();
        }
    }

    std::string readString(const std::string& prompt)
    {
        std::string val;
        std::cout << prompt;
        std::getline(std::cin, val);
        return val;
    }

public:
    ConsoleFacade(std::shared_ptr<POSApplication> a): app(a) {}

    void runInteractive()
    {
        while(true)
        {
            std::cout << "\nМЕНЮ МОДУЛЯ ПОС\n"
                      << "1. Добавить студента\n"
                      << "2. Добавить критерий\n"
                      << "3. Показать всех студентов и критерии\n"
                      << "4. Выставить/Исправить оценку\n"
                      << "5. Посмотреть оценки студента\n"
                      << "6. Удалить оценку\n"
                      << "7. Посчитать итоговый балл\n"
                      << "8. Экспортировать итоги всех студентов\n"
                      << "9. Запустить демо-сценарий\n"
                      << "0. Выход\n"
                      << "Выберите действие: ";

            int choice;
            if(!(std::cin >> choice))
            {
                std::cout << "Ошибка ввода.\n";
                clearInput();
                continue;
            }
            std::cout << "\n";
            clearInput();

            try
            {
                if(choice == 1)
                {
                    std::string id = readString("ID студента: ");
                    std::string name = readString("Имя: ");
                    app->addStudent(id, name);
                    std::cout << "Студент добавлен.\n";
                }
                else if(choice == 2)
                {
                    std::string id = readString("ID критерия: ");
                    std::string name = readString("Название: ");
                    double max_s = readDouble("Макс. балл: ");
                    double weight = readDouble("Вес (0..1): ");
                    app->addCriterion(id, name, max_s, weight);
                    std::cout << "Критерий добавлен.\n";
                }
                else if(choice == 3)
                {
                    std::cout << "СТУДЕНТЫ\n";
                    for(const auto& s: app->getAllStudents())
                        std::cout << s.id << ": " << s.name << "\n";
                    std::cout << "\nКРИТЕРИИ\n";
                    for(const auto& [id, c]: app->getAllCriteria())
                        std::cout << id << ": " << c.name
                                  << " (макс: " << c.max_score
                                  << ", вес: " << c.weight << ")\n";
                }
                else if(choice == 4)
                {
                    std::string sid = readString("ID студента: ");
                    std::string cid = readString("ID критерия: ");
                    double score = readDouble("Балл: ");
                    app->gradeStudent(sid, cid, score);
                    std::cout << "Оценка сохранена.\n";
                }
                else if(choice == 5)
                {
                    std::string sid = readString("ID студента: ");
                    auto evals = app->getStudentEvals(sid);
                    if(evals.empty())
                        std::cout << "Оценок нет.\n";
                    for(const auto& e: evals)
                        std::cout << "Критерий " << e.criterion_id << ": "
                                  << e.score << "\n";
                }
                else if(choice == 6)
                {
                    std::string sid = readString("ID студента: ");
                    std::string cid = readString("ID критерия: ");
                    app->removeEvaluation(sid, cid);
                    std::cout << "Оценка удалена.\n";
                }
                else if(choice == 7)
                {
                    std::string sid = readString("ID студента: ");
                    auto st = app->getStudent(sid);
                    if(st)
                    {
                        auto fg = app->getStudentFinalGrade(sid);
                        std::cout << "Итоговый балл " << st->name << ": "
                                  << fg.total_score << "\n";
                    }
                    else
                        std::cout << "Студент не найден.\n";
                }
                else if(choice == 8)
                {
                    std::string filename = readString(
                        "Имя файла для сохранения (например, results.csv): ");
                    app->exportAllGrades(filename);
                    std::cout << "Успешно экспортировано в " << filename
                              << "\n";
                }
                else if(choice == 9)
                {
                    runDemo();
                }
                else if(choice == 0)
                {
                    break;
                }
            }
            catch(const std::exception& e)
            {
                std::cout << "[Ошибка]: " << e.what() << "\n";
            }
        }
    }

    void runDemo()
    {
        std::cout << "\nДЕМО-СЦЕНАРИЙ\n";
        app->addStudent("s1", "Иван Иванов");
        app->addStudent("s2", "Мария Петрова");

        app->addCriterion("c1", "Домашнее задание", 10, 0.4);
        app->addCriterion("c2", "Экзамен", 100, 0.6);

        std::cout << "Выставляем оценки....\n";
        app->gradeStudent("s1", "c1", 8);
        app->gradeStudent("s1", "c2", 80);
        app->gradeStudent("s2", "c1", 10);
        app->gradeStudent("s2", "c2", 95);

        try
        {
            app->gradeStudent("s1", "c1", 12);
        }
        catch(const std::exception& e)
        {
            std::cout << "[Ошибка Домена]: " << e.what() << "\n";
        }

        auto fg1 = app->getStudentFinalGrade("s1");
        auto fg2 = app->getStudentFinalGrade("s2");
        std::cout << "Итоговый балл Ивана: " << fg1.total_score << "\n";
        std::cout << "Итоговый балл Марии: " << fg2.total_score << "\n";

        std::cout
            << "Экспорт ведомости всех студентов в demo_results.csv....\n";
        app->exportAllGrades("demo_results.csv");
        std::cout << "----------------------\n";
    }
};

int main()
{
    auto studentRepo = std::make_shared<InMemoryStudentRepo>();
    auto rubricRepo = std::make_shared<InMemoryRubricRepo>();
    auto evalRepo = std::make_shared<InMemoryEvalRepo>();
    auto strategy = std::make_shared<WeightedGradeStrategy>();
    auto exporter = std::make_shared<TxtGradeExporter>();

    auto app = std::make_shared<POSApplication>(studentRepo, rubricRepo,
                                                evalRepo, strategy, exporter);

    ConsoleFacade ui(app);
    ui.runInteractive();

    return 0;
}
