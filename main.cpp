#include "mainwindow.h"
#include <QApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>

static bool connectToDatabase(QSqlDatabase &db)
{
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("127.0.0.1");
    db.setPort(5432);
    db.setDatabaseName("tool_tracker");
    db.setUserName("postgres");
    db.setPassword("postgres");

    if (!db.open()) {
        QMessageBox::critical(nullptr, "Ошибка подключения",
                              "Не удалось подключиться к базе данных:\n" + db.lastError().text());
        return false;
    }
    qDebug() << "Подключение к PostgreSQL установлено.";
    return true;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QSqlDatabase db;
    if (!connectToDatabase(db))
        return 1;

    QStringList roles;
    roles << "Кладовщик (Полный доступ)" << "Сотрудник (Только просмотр)";

    bool ok;
    QString role = QInputDialog::getItem(nullptr, "Выбор роли",
                                         "Под каким пользователем вы хотите войти?",
                                         roles, 0, false, &ok);

    if (!ok || role.isEmpty())
        return 0;

    bool isAdmin = (role == roles[0]);

    MainWindow w(isAdmin);
    w.show();
    return a.exec();
}