#include "mainwindow.hpp"

#include "ui_untitled.h"
#include <iostream>

static QString size_human(float num) {
	QStringList list;
	list << "KB"
	     << "MB"
	     << "GB"
	     << "TB";
	QStringListIterator i(list);
	QString unit("bytes");
	while (num >= 1024.0 && i.hasNext()) {
		unit = i.next();
		num /= 1024.0;
	}
	return QString().setNum(num, 'f', 2) + " " + unit;
}

MainWindow::MainWindow(QWidget *parent)
    : startEnabled{true}, ui{std::make_unique<Ui::MainWindow>()},
      walker{std::make_unique<FtpWalker>()}, status{std::make_unique<QLabel>(
                                                 this)} {
	ui->setupUi(this);
	statusBar()->addWidget(status.get());
	connect(this, &MainWindow::startGrabbing, walker.get(), &FtpWalker::start);
	connect(this, &MainWindow::stopGrabbing, walker.get(), &FtpWalker::stop);
	connect(walker.get(), &FtpWalker::foundItem, this,
	        [&](QString name, size_t size) {
		        std::cout << name.toStdString() << " " << size << std::endl;
		        if (size > 0) {
			        auto stdname = name.toStdString();
			        if (stdname.rfind('.') != stdname.rfind('/') + 1 &&
			            stdname.rfind('.') != std::string::npos) {
				        extensionSizes[stdname.substr(stdname.rfind('.'))] += size;
			        }

			        ui->tableWidget->setSortingEnabled(false);
			        ui->tableWidget->insertRow(ui->tableWidget->rowCount());
			        auto id = ui->tableWidget->rowCount() - 1;
			        ui->tableWidget->setItem(id, 1, new QTableWidgetItem(name));
			        ui->tableWidget->setItem(id, 0,
			                                 new QTableWidgetItem(size_human(size)));
			        ui->tableWidget->setSortingEnabled(true);
		        }
	        },
	        Qt::QueuedConnection);
	connect(walker.get(), &FtpWalker::progress, this,
	        [&](size_t current, size_t all) {
		        // std::cout << current << " " << all << std::endl;
		        status->setText(QString::number(current) + "/" +
		                        QString::number(all));
	        },
	        Qt::QueuedConnection);
	connect(walker.get(), &FtpWalker::started, this,
	        [&]() {
		        status->setText("Looking up");
		        ui->tableWidget->setRowCount(0);
		        startEnabled = false;
		        ui->startButton->setText("Stop");
		        ui->hostEdit->setEnabled(false);
		        ui->portEdit->setEnabled(false);
		        ui->loginEdit->setEnabled(false);
		        ui->passwordEdit->setEnabled(false);
	        },
	        Qt::QueuedConnection);
	connect(walker.get(), &FtpWalker::finished, this,
	        [&]() {
		        status->setText("Finished " + status->text());
		        startEnabled = true;
		        ui->startButton->setText("Start");
		        ui->hostEdit->setEnabled(true);
		        ui->portEdit->setEnabled(true);
		        ui->loginEdit->setEnabled(true);
		        ui->passwordEdit->setEnabled(true);
		        for (auto &&ext : extensionSizes) {
			        std::cout << ext.first << ' ' << size_human(ext.second).toStdString()
			                  << std::endl;
		        }
	        },
	        Qt::QueuedConnection);
	connect(ui->startButton, &QPushButton::clicked, [=, this] {
		if (startEnabled) {
			emit startGrabbing(
			    {ui->hostEdit->text().toStdString(), ui->portEdit->value()},
			    {ui->loginEdit->text().toStdString(),
			     ui->passwordEdit->text().toStdString()});
		} else {
			emit stopGrabbing();
		}
	});
}
MainWindow::~MainWindow() {}
