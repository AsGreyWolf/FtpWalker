#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "FtpWalker.hpp"
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <memory>

namespace Ui {
class MainWindow;
}

class FtpWalker;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow() override;

private:
	bool startEnabled;
	std::unique_ptr<Ui::MainWindow> ui;
	std::unique_ptr<FtpWalker> walker;
	std::unique_ptr<QLabel> status;

signals:
	void startGrabbing(const FtpWalker::HostInfo &host,
	                   const FtpWalker::AuthInfo &auth);
	void stopGrabbing();
};

#endif // MAINWINDOW_H
