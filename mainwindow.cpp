#include "mainwindow.h"
#include "xmltoh.h"
#include "./ui_mainwindow.h"

QString lastUsedPath = QDir::homePath();

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_addFileButton_clicked()
{
    QString inFile = QFileDialog::getOpenFileName(
                this,
                "Выберите файл",
                lastUsedPath,
                "XML (*.xml)"
                );

    if (inFile.isEmpty())
    {
        return;
    }

    lastUsedPath = inFile;
    if (ui->textEdit->toPlainText().isEmpty())
        ui->textEdit->insertPlainText(inFile);
    else
        ui->textEdit->insertPlainText("\n" + inFile);
}

void MainWindow::on_converteFileButton_clicked()
{
    if (ui->textEdit->toPlainText().isEmpty())
    {
        QMessageBox::information(this, "Info", "Ни один файл не выбран");
        return;
    }

    QString outDir = QFileDialog::getExistingDirectory(
                this,
                "Выберите папку для сохранения",
                lastUsedPath
                );

    if (outDir.isEmpty())
    {
        return;
    }

    lastUsedPath = outDir;

    QStringList inFilePaths = ui->textEdit->toPlainText().split("\n");

    foreach (QString inFilePath, inFilePaths)
    {
        QFileInfo inFileInfo(inFilePath);
        QString baseName = inFileInfo.baseName();
        QString outFilePath = outDir + "/" + baseName + ".h";

        if (!ConverteXMLtoH(inFilePath, outFilePath))
            QMessageBox::warning(this, "Warning", "Не удалось конвертировать файл:\n " + inFilePath);
    }
    QMessageBox::information(this, "Info", "Все файлы конвертированы");
}

void MainWindow::on_deleteFileButton_clicked()
{
    QString text = ui->textEdit->toPlainText();
    if (text.isEmpty())
        return;
    QStringList lines = text.split('\n');
    lines.removeLast();
    QString newText = lines.join('\n');
    ui->textEdit->clear();
    ui->textEdit->insertPlainText(newText);
}

void MainWindow::on_deleteAllFileButton_clicked()
{
    ui->textEdit->clear();
}
