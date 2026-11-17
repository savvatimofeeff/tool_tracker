#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QStringList>

class QTabWidget;
class QTableView;
class QPushButton;
class QLineEdit;
class QLabel;
class QHBoxLayout;
class QSqlTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool isAdmin, QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onRefreshAll();
    void onCheckServiceLife();
    void onIssueTool();
    void onReturnTool();
    void onSendToMaintenance();
    void onAddRecord(const QString &tableName);

private:
    struct TabPage {
        QWidget        *widget     = nullptr;
        QTableView     *view       = nullptr;
        QLineEdit      *filter     = nullptr;
        QPushButton    *addBtn     = nullptr;
        QPushButton    *delBtn     = nullptr;
        QPushButton    *saveBtn    = nullptr;
        QHBoxLayout    *topLayout  = nullptr;
        QSqlTableModel *model      = nullptr;
        QString         filterField;
    };

    void buildUi();
    TabPage *createTab(const QString &table,
                       const QStringList &hiddenColumns,
                       const QString &filterField);
    void refreshStatuses();

    void addEmployeeDialog();
    void addToolDialog();

    bool isAdminMode;

    QTabWidget  *tabWidget    = nullptr;
    QPushButton *btnRefresh   = nullptr;
    QPushButton *btnCheckLife = nullptr;
    QLabel      *statusLabel  = nullptr;

    TabPage *tabEmployees   = nullptr;
    TabPage *tabTools       = nullptr;
    TabPage *tabIssues      = nullptr;
    TabPage *tabReturns     = nullptr;
    TabPage *tabMaintenance = nullptr;

    QList<TabPage*> pages;
};

#endif // MAINWINDOW_H