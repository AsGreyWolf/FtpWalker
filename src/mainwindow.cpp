#include "mainwindow.hpp"

#include "HttpWalker.hpp"
#include "app.hpp"
#include "ui_untitled.h"
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : ui{std::make_unique<Ui::MainWindow>()},
      walker{std::make_unique<HttpWalker>()} {
	ui->setupUi(this);
	connect(this, &MainWindow::startGrabbing, walker.get(), &HttpWalker::start);
	connect(walker.get(), &HttpWalker::foundItem, this,
	        [&](QString url, unsigned short code) {
		        if (code != 200) {
			        // std::cout << url << " " << code << std::endl;
			        ui->listWidget->addItem(url + " " + QString::number(code));
		        }
	        },
	        Qt::QueuedConnection);
	connect(walker.get(), &HttpWalker::progress, this,
	        [&](size_t current, size_t all) {
		        // std::cout << current << " " << all << std::endl;
		        ui->progressBar->setMaximum(all);
		        ui->progressBar->setValue(current);
	        },
	        Qt::QueuedConnection);
	connect(ui->startButton, &QPushButton::clicked, [=] {
		std::string url = ui->urlEdit->text().toStdString();
		emit startGrabbing(url);
	});
}
MainWindow::~MainWindow() {}
