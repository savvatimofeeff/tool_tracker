#include "mainwindow.h"

#include <QApplication>
#include <QTabWidget>
#include <QTableView>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDateEdit>
#include <QTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTime>
#include <QSpinBox> // Для ввода срока службы

MainWindow::MainWindow(bool isAdmin, QWidget *parent)
    : QMainWindow(parent), isAdminMode(isAdmin)
{
    buildUi();
    refreshStatuses();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    QString title = isAdminMode ? "Учёт инструмента (Кладовщик)" : "Учёт инструмента (Сотрудник)";
    setWindowTitle(title);
    resize(1180, 720);

    tabWidget = new QTabWidget(this);

    tabEmployees   = createTab("employee",     {"id_employee"},    "surname");
    tabTools       = createTab("tool",         {"id_tool"},        "name_tool");
    tabIssues      = createTab("tool_issue",   {"id_issue"},       "id_tool");
    tabReturns     = createTab("tool_return",  {"id_return"},      "id_issue");
    tabMaintenance = createTab("maintenance",  {"id_maintenance"}, "work_type");

    // Добавляем вкладки
    if (isAdminMode) {
        tabWidget->addTab(tabEmployees->widget,   "Сотрудники");
    }
    tabWidget->addTab(tabTools->widget,       "Инструмент");
    tabWidget->addTab(tabIssues->widget,      "Выдачи");
    tabWidget->addTab(tabReturns->widget,     "Возвраты");
    tabWidget->addTab(tabMaintenance->widget, "Заточка/Ремонт");

    // Кнопки бизнес-операций (только для админа)
    if (isAdminMode) {
        auto *btnIssue = new QPushButton("Выдать инструмент");
        auto *btnReturn = new QPushButton("Оформить возврат");
        auto *btnMaint = new QPushButton("Отправить на заточку/ремонт");

        tabIssues->topLayout->addWidget(btnIssue);
        tabReturns->topLayout->addWidget(btnReturn);
        tabMaintenance->topLayout->addWidget(btnMaint);

        connect(btnIssue, &QPushButton::clicked, this, &MainWindow::onIssueTool);
        connect(btnReturn, &QPushButton::clicked, this, &MainWindow::onReturnTool);
        connect(btnMaint, &QPushButton::clicked, this, &MainWindow::onSendToMaintenance);
    }

    btnRefresh   = new QPushButton("Обновить всё");
    btnCheckLife = new QPushButton("Проверить сроки службы");
    statusLabel  = new QLabel("Готово");

    auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(central);
    auto *top = new QHBoxLayout;
    top->addWidget(btnRefresh);

    if (isAdminMode) {
        top->addWidget(btnCheckLife);
    }

    top->addStretch();
    top->addWidget(statusLabel);
    v->addLayout(top);
    v->addWidget(tabWidget);
    setCentralWidget(central);

    connect(btnRefresh,   &QPushButton::clicked, this, &MainWindow::onRefreshAll);
    connect(btnCheckLife, &QPushButton::clicked, this, &MainWindow::onCheckServiceLife);

    // Скрываем кнопки редактирования, если вошел Сотрудник
    if (!isAdminMode) {
        for (auto *p : pages) {
            p->addBtn->hide();
            p->delBtn->hide();
            p->saveBtn->hide();
        }
    }
}

MainWindow::TabPage* MainWindow::createTab(const QString &table,
                                           const QStringList &hiddenColumns,
                                           const QString &filterField)
{
    auto *page = new TabPage();
    page->filterField = filterField;

    page->model = new QSqlTableModel(this);
    page->model->setTable(table);
    page->model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    page->model->select();

    page->view = new QTableView;
    page->view->setModel(page->model);
    page->view->setSelectionBehavior(QAbstractItemView::SelectRows);
    page->view->setSelectionMode(QAbstractItemView::SingleSelection);
    page->view->setAlternatingRowColors(true);
    page->view->horizontalHeader()->setStretchLastSection(true);
    page->view->setEditTriggers(QAbstractItemView::NoEditTriggers); // Запрет редактирования в таблице

    for (const QString &col : hiddenColumns) {
        int idx = page->model->record().indexOf(col);
        if (idx >= 0)
            page->view->setColumnHidden(idx, true);
    }

    page->filter = new QLineEdit;
    page->filter->setPlaceholderText("Поиск по «" + filterField + "»…");

    page->addBtn  = new QPushButton("Добавить");
    page->delBtn  = new QPushButton("Удалить");
    page->saveBtn = new QPushButton("Сохранить");

    page->topLayout = new QHBoxLayout;
    page->topLayout->addWidget(new QLabel("Фильтр:"));
    page->topLayout->addWidget(page->filter);
    page->topLayout->addStretch();
    page->topLayout->addWidget(page->addBtn);
    page->topLayout->addWidget(page->delBtn);
    page->topLayout->addWidget(page->saveBtn);

    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->addLayout(page->topLayout);
    v->addWidget(page->view);
    page->widget = w;

    // Кнопка "Добавить" вызывает нужный диалог в зависимости от таблицы
    connect(page->addBtn, &QPushButton::clicked, this, [this, table]() {
        onAddRecord(table);
    });

    connect(page->delBtn, &QPushButton::clicked, this, [this, page]() {
        QModelIndex idx = page->view->currentIndex();
        if (!idx.isValid()) return;
        if (QMessageBox::question(this, "Удаление",
                                  "Удалить выбранную запись?") != QMessageBox::Yes)
            return;
        page->model->removeRow(idx.row());
        if (page->model->submitAll()) {
            page->model->select();
            refreshStatuses();
        } else {
            QMessageBox::warning(this, "Ошибка удаления",
                                 page->model->lastError().text());
            page->model->revertAll();
        }
    });

    connect(page->saveBtn, &QPushButton::clicked, this, [this, page]() {
        if (page->model->submitAll()) {
            page->model->select();
            refreshStatuses();
            statusLabel->setText("Сохранено: " +
                                 QTime::currentTime().toString("hh:mm:ss"));
        } else {
            QMessageBox::warning(this, "Ошибка сохранения",
                                 page->model->lastError().text());
        }
    });

    connect(page->filter, &QLineEdit::textChanged, this, [page](const QString &text) {
        QString t = text;
        t.replace("'", "''");
        if (t.isEmpty())
            page->model->setFilter("");
        else
            page->model->setFilter(
                QString("CAST(%1 AS TEXT) ILIKE '%%2%%'")
                    .arg(page->filterField, t));
        page->model->select();
    });

    pages.append(page);
    return page;
}

// ==========================================================
//  Маршрутизация добавления по названию таблицы
// ==========================================================
void MainWindow::onAddRecord(const QString &tableName)
{
    if (tableName == "employee") {
        addEmployeeDialog();
    } else if (tableName == "tool") {
        addToolDialog();
    } else if (tableName == "tool_issue") {
        onIssueTool();
    } else if (tableName == "tool_return") {
        onReturnTool();
    } else if (tableName == "maintenance") {
        onSendToMaintenance();
    }
}

// ==========================================================
//  Диалог добавления сотрудника
// ==========================================================
void MainWindow::addEmployeeDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Добавить сотрудника");
    dlg.resize(350, 300);
    auto *form = new QFormLayout(&dlg);

    auto *leSurname = new QLineEdit(&dlg);
    auto *leName = new QLineEdit(&dlg);
    auto *lePatronymic = new QLineEdit(&dlg);
    auto *lePosition = new QLineEdit(&dlg);
    auto *lePhone = new QLineEdit(&dlg);
    auto *leEmail = new QLineEdit(&dlg);

    form->addRow("Фамилия*:", leSurname);
    form->addRow("Имя*:", leName);
    form->addRow("Отчество:", lePatronymic);
    form->addRow("Должность*:", lePosition);
    form->addRow("Телефон:", lePhone);
    form->addRow("Email:", leEmail);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (leSurname->text().trimmed().isEmpty() ||
            leName->text().trimmed().isEmpty() ||
            lePosition->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Поля со звёздочкой (*) обязательны для заполнения.");
            return;
        }

        QSqlQuery ins;
        ins.prepare("INSERT INTO employee (surname, name, patronymic, position, phone, email) "
                    "VALUES (:sn, :n, :p, :pos, :ph, :e)");
        ins.bindValue(":sn", leSurname->text());
        ins.bindValue(":n", leName->text());
        ins.bindValue(":p", lePatronymic->text().isEmpty() ? QVariant() : lePatronymic->text());
        ins.bindValue(":pos", lePosition->text());
        ins.bindValue(":ph", lePhone->text().isEmpty() ? QVariant() : lePhone->text());
        ins.bindValue(":e", leEmail->text().isEmpty() ? QVariant() : leEmail->text());

        if (ins.exec()) {
            tabEmployees->model->select();
            statusLabel->setText("Сотрудник добавлен: " + leSurname->text());
        } else {
            QMessageBox::warning(this, "Ошибка", ins.lastError().text());
        }
    }
}

// ==========================================================
//  Диалог добавления инструмента
// ==========================================================
void MainWindow::addToolDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Добавить инструмент");
    dlg.resize(400, 350);
    auto *form = new QFormLayout(&dlg);

    auto *leName = new QLineEdit(&dlg);
    auto *leCategory = new QLineEdit(&dlg);
    auto *leUnit = new QLineEdit(&dlg);

    auto *deCommission = new QDateEdit(QDate::currentDate(), &dlg);
    deCommission->setCalendarPopup(true);
    deCommission->setDisplayFormat("dd.MM.yyyy");

    auto *sbLife = new QSpinBox(&dlg);
    sbLife->setRange(1, 10000);
    sbLife->setValue(365);

    form->addRow("Название*:", leName);
    form->addRow("Категория*:", leCategory);
    form->addRow("Ед. измерения* (шт/компл):", leUnit);
    form->addRow("Дата ввода в эксплуатацию:", deCommission);
    form->addRow("Срок службы (дней)*:", sbLife);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (leName->text().trimmed().isEmpty() ||
            leCategory->text().trimmed().isEmpty() ||
            leUnit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Поля со звёздочкой (*) обязательны для заполнения.");
            return;
        }

        QSqlQuery ins;
        ins.prepare("INSERT INTO tool (name_tool, category, unit, commission_date, service_life_days, status) "
                    "VALUES (:n, :c, :u, :d, :l, 'available')");
        ins.bindValue(":n", leName->text());
        ins.bindValue(":c", leCategory->text());
        ins.bindValue(":u", leUnit->text());
        ins.bindValue(":d", deCommission->date());
        ins.bindValue(":l", sbLife->value());

        if (ins.exec()) {
            tabTools->model->select();
            statusLabel->setText("Инструмент добавлен: " + leName->text());
        } else {
            QMessageBox::warning(this, "Ошибка", ins.lastError().text());
        }
    }
}

void MainWindow::refreshStatuses()
{
    QSqlQuery q;
    bool ok = q.exec(R"(
        UPDATE tool t SET status = CASE
            WHEN EXISTS (SELECT 1 FROM maintenance m
                         WHERE m.id_tool = t.id_tool
                           AND m.returned_date IS NULL)
                 THEN 'maintenance'
            WHEN EXISTS (SELECT 1 FROM tool_issue i
                         LEFT JOIN tool_return r ON r.id_issue = i.id_issue
                         WHERE i.id_tool = t.id_tool
                           AND r.id_return IS NULL)
                 THEN 'issued'
            ELSE 'available'
        END
        WHERE t.status <> 'written_off'
    )");

    if (!ok) {
        qWarning() << "refreshStatuses error:" << q.lastError().text();
    }

    if (tabTools)
        tabTools->model->select();
}

void MainWindow::onRefreshAll()
{
    for (auto *p : pages)
        p->model->select();
    refreshStatuses();
    statusLabel->setText("Обновлено: " + QTime::currentTime().toString("hh:mm:ss"));
}

void MainWindow::onCheckServiceLife()
{
    QSqlQuery q;
    q.exec(R"(
        SELECT name_tool,
               commission_date + service_life_days * INTERVAL '1 day' AS expiry,
               ((commission_date + service_life_days * INTERVAL '1 day')::date
                    - CURRENT_DATE) AS days_left
        FROM tool
        WHERE status <> 'written_off'
          AND commission_date + service_life_days * INTERVAL '1 day'
              <= CURRENT_DATE + INTERVAL '30 days'
        ORDER BY expiry
    )");

    QStringList lines;
    while (q.next()) {
        int d = q.value("days_left").toInt();
        QString state = (d < 0)
                            ? QString("ПРОСРОЧЕН на %1 дн.").arg(-d)
                            : QString("истекает через %1 дн.").arg(d);
        lines << QString("• %1 — %2")
                     .arg(q.value("name_tool").toString(), state);
    }

    if (lines.isEmpty())
        QMessageBox::information(this, "Контроль сроков службы",
                                 "Инструмент с истекающим или истёкшим сроком службы не обнаружен.");
    else
        QMessageBox::warning(this, "Контроль сроков службы",
                             "Требуют внимания:\n\n" + lines.join("\n"));
}

void MainWindow::onIssueTool()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Выдать инструмент");
    dlg.resize(450, 320);
    auto *form = new QFormLayout(&dlg);

    auto *cbTool = new QComboBox(&dlg);
    QSqlQuery qt("SELECT id_tool, name_tool, unit FROM tool "
                 "WHERE status = 'available' ORDER BY name_tool");
    while (qt.next())
        cbTool->addItem(QString("%1 (%2)")
                            .arg(qt.value("name_tool").toString(),
                                 qt.value("unit").toString()),
                        qt.value("id_tool").toInt());

    auto *cbEmp = new QComboBox(&dlg);
    QSqlQuery qe("SELECT id_employee, "
                 "surname || ' ' || name || ' ' || COALESCE(patronymic,'') AS fio "
                 "FROM employee ORDER BY surname");
    while (qe.next())
        cbEmp->addItem(qe.value("fio").toString(),
                       qe.value("id_employee").toInt());

    auto *deReturn = new QDateEdit(QDate::currentDate().addDays(7), &dlg);
    deReturn->setCalendarPopup(true);
    deReturn->setDisplayFormat("dd.MM.yyyy");

    auto *leCond  = new QLineEdit(&dlg);
    auto *teNotes = new QTextEdit(&dlg);
    teNotes->setMaximumHeight(70);

    form->addRow("Инструмент:", cbTool);
    form->addRow("Сотрудник:",  cbEmp);
    form->addRow("Планируемая дата возврата:", deReturn);
    form->addRow("Состояние при выдаче:", leCond);
    form->addRow("Примечание:",  teNotes);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (cbTool->count() == 0) {
        QMessageBox::information(this, "Нет доступного инструмента",
                                 "Все инструменты выданы или находятся в ремонте.");
        return;
    }

    if (dlg.exec() != QDialog::Accepted) return;

    QSqlQuery ins;
    ins.prepare("INSERT INTO tool_issue"
                "(id_tool, id_employee, planned_return_date, condition_on_issue, notes) "
                "VALUES (:t, :e, :r, :c, :n)");
    ins.bindValue(":t", cbTool->currentData().toInt());
    ins.bindValue(":e", cbEmp->currentData().toInt());
    ins.bindValue(":r", deReturn->date());
    ins.bindValue(":c", leCond->text().isEmpty() ? QVariant() : leCond->text());
    ins.bindValue(":n", teNotes->toPlainText().isEmpty() ? QVariant() : teNotes->toPlainText());

    if (!ins.exec()) {
        QMessageBox::warning(this, "Ошибка", ins.lastError().text());
        return;
    }

    refreshStatuses();
    tabIssues->model->select();
    statusLabel->setText("Инструмент выдан: " + cbTool->currentText());
}

void MainWindow::onReturnTool()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Оформить возврат");
    dlg.resize(520, 260);
    auto *form = new QFormLayout(&dlg);

    auto *cbIssue = new QComboBox(&dlg);
    QSqlQuery q(R"(
        SELECT i.id_issue,
               t.name_tool || ' — ' || e.surname || ' ' || e.name
                   || ' (' || to_char(i.issue_date,'DD.MM.YYYY') || ')' AS label
        FROM tool_issue i
        JOIN tool t     ON t.id_tool = i.id_tool
        JOIN employee e ON e.id_employee = i.id_employee
        LEFT JOIN tool_return r ON r.id_issue = i.id_issue
        WHERE r.id_return IS NULL
        ORDER BY i.issue_date DESC
    )");
    while (q.next())
        cbIssue->addItem(q.value("label").toString(),
                         q.value("id_issue").toInt());

    auto *leCond  = new QLineEdit(&dlg);
    auto *teNotes = new QTextEdit(&dlg);
    teNotes->setMaximumHeight(70);

    form->addRow("Открытая выдача:", cbIssue);
    form->addRow("Состояние при возврате:", leCond);
    form->addRow("Примечание:", teNotes);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (cbIssue->count() == 0) {
        QMessageBox::information(this, "Нет открытых выдач",
                                 "Нет инструмента, ожидающего возврата.");
        return;
    }

    if (dlg.exec() != QDialog::Accepted) return;

    QSqlQuery ins;
    ins.prepare("INSERT INTO tool_return(id_issue, condition_on_return, notes) "
                "VALUES (:i, :c, :n)");
    ins.bindValue(":i", cbIssue->currentData().toInt());
    ins.bindValue(":c", leCond->text().isEmpty() ? QVariant() : leCond->text());
    ins.bindValue(":n", teNotes->toPlainText().isEmpty() ? QVariant() : teNotes->toPlainText());

    if (!ins.exec()) {
        QMessageBox::warning(this, "Ошибка", ins.lastError().text());
        return;
    }

    refreshStatuses();
    tabReturns->model->select();
    tabIssues->model->select();
    statusLabel->setText("Возврат оформлен.");
}

void MainWindow::onSendToMaintenance()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Отправить на заточку/ремонт");
    dlg.resize(450, 320);
    auto *form = new QFormLayout(&dlg);

    auto *cbTool = new QComboBox(&dlg);
    QSqlQuery qt("SELECT id_tool, name_tool FROM tool "
                 "WHERE status IN ('available','written_off') ORDER BY name_tool");
    while (qt.next())
        cbTool->addItem(qt.value("name_tool").toString(),
                        qt.value("id_tool").toInt());

    auto *cbType = new QComboBox(&dlg);
    cbType->addItems({"заточка", "ремонт", "регулировка"});

    auto *leCost  = new QLineEdit(&dlg);
    leCost->setPlaceholderText("например, 350.00");
    auto *teNotes = new QTextEdit(&dlg);
    teNotes->setMaximumHeight(70);

    form->addRow("Инструмент:", cbTool);
    form->addRow("Тип работ:", cbType);
    form->addRow("Стоимость:", leCost);
    form->addRow("Примечание:", teNotes);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (cbTool->count() == 0) {
        QMessageBox::information(this, "Нет инструмента",
                                 "Нет доступного инструмента для отправки в ремонт.");
        return;
    }

    if (dlg.exec() != QDialog::Accepted) return;

    QSqlQuery ins;
    ins.prepare("INSERT INTO maintenance(id_tool, work_type, cost, notes) "
                "VALUES (:t, :w, :c, :n)");
    ins.bindValue(":t", cbTool->currentData().toInt());
    ins.bindValue(":w", cbType->currentText());
    if (leCost->text().trimmed().isEmpty())
        ins.bindValue(":c", QVariant());
    else
        ins.bindValue(":c", leCost->text().toDouble());
    ins.bindValue(":n", teNotes->toPlainText().isEmpty() ? QVariant() : teNotes->toPlainText());

    if (!ins.exec()) {
        QMessageBox::warning(this, "Ошибка", ins.lastError().text());
        return;
    }

    refreshStatuses();
    tabMaintenance->model->select();
    statusLabel->setText("Инструмент отправлен: " + cbTool->currentText());
}