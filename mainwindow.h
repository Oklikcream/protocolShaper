#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_addFileButton_clicked();

    void on_converteFileToHButton_clicked();

    void on_deleteFileButton_clicked();

    void on_deleteAllFileButton_clicked();

    void on_converteFileToDocxButton_clicked();

private:
    Ui::MainWindow *ui;
};


#endif // MAINWINDOW_H
