#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QGraphicsDropShadowEffect>
#include <QColor>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 원본 이미지 그림자 효과
    auto *originalShadow = new QGraphicsDropShadowEffect();

    originalShadow->setBlurRadius(20);
    originalShadow->setXOffset(0);
    originalShadow->setYOffset(5);
    originalShadow->setColor(QColor(0, 0, 0, 100));

    ui->originalImageLabel->setGraphicsEffect(originalShadow);


    // 보정 이미지 그림자 효과
    auto *processedShadow = new QGraphicsDropShadowEffect();

    processedShadow->setBlurRadius(20);
    processedShadow->setXOffset(0);
    processedShadow->setYOffset(5);
    processedShadow->setColor(QColor(0, 0, 0, 100));

    ui->ProcessedImageLabel->setGraphicsEffect(processedShadow);

}

MainWindow::~MainWindow()
{
    delete ui;
}